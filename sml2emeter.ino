/**
* ESP8266 SML to SMA energy meter converter
*
* ﻿This sketch may be used to read SML telegrams from an infrared D0 interface, convert it to SMA energy-meter 
* telegrams and send it via UDP.
*
* Configuration:
* Copy the settings.h.tpl file to settings.h and check the default settings.
* At least the WiFi settings have to be adapted to your network settings.
*
* For more details, see the readme:
* https://github.com/jtuemmler/sml2emeter/blob/master/readme.adoc
*/

#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include "util/sml_testpacket.h"

// ----------------------------------------------------------------------------
// Settings
// ----------------------------------------------------------------------------

#include "settings.h"

// ----------------------------------------------------------------------------
// SML constants
// For details see:
// https://www.bsi.bund.de/SharedDocs/Downloads/DE/BSI/Publikationen/TechnischeRichtlinien/TR03109/TR-03109-1_Anlage_Feinspezifikation_Drahtgebundene_LMN-Schnittstelle_Teilb.pdf?__blob=publicationFile
// ----------------------------------------------------------------------------
const uint32_t SML_ESCAPE = 0x1b1b1b1b;
const uint32_t SML_VERSION1 = 0x01010101;
const uint8_t SML_TAG_MASK = 0x70;
const uint8_t SML_LENGTH_MASK = 0x0f;
const uint8_t SML_LIST_TAG = 0x70;

const uint8_t SML_ID_PATTERN[] = { 0x77, 0x07, 0x01, 0x00 };

const int SML_MIN_SCALE = -2;
const int SML_MAX_SCALE = 5;
const int SML_SCALE_VALUES = SML_MAX_SCALE - SML_MIN_SCALE + 1;

// Energy is in Ws                                        -2  -1   0     1      2       3        4         5
const uint64_t SCALE_FACTORS_ENERGY[SML_SCALE_VALUES] = { 36, 360, 3600, 36000, 360000, 3600000, 36000000, 360000000 };

// Power is in 0,1 W                                    -2  -1  0   1    2     3      4       5
const int32_t SCALE_FACTORS_POWER[SML_SCALE_VALUES] = { -10, 1, 10, 100, 1000, 10000, 100000, 1000000 };

// ----------------------------------------------------------------------------
// OBIS constants
//
// For details see:
// https://www.promotic.eu/en/pmdoc/Subsystems/Comm/PmDrivers/IEC62056_OBIS.htm
// https://www.bundesnetzagentur.de/DE/Service-Funktionen/Beschlusskammern/BK06/BK6_81_GPKE_GeLi/Mitteilung_Nr_20/Anlagen/Obis-Kennzahlen-System_2.0.pdf?__blob=publicationFile&v=2
// ----------------------------------------------------------------------------
const uint8_t OBIS_INSTANTANEOUS_POWER_TYPE = 7;
const uint8_t OBIS_ENERGY_TYPE = 8;
const uint8_t OBIS_POSITIVE_ACTIVE_POWER = 1;
const uint8_t OBIS_NEGATIVE_ACTIVE_POWER = 2;
const uint8_t OBIS_SUM_ACTIVE_POWER = 16;

// ----------------------------------------------------------------------------
// SMA energy meter constants
//
// For details see:
// https://www.sma.de/fileadmin/content/global/Partner/Documents/SMA_Labs/EMETER-Protokoll-TI-de-10.pdf
// ----------------------------------------------------------------------------

// IDs to identify values in the energy meter packets
const uint32_t SMA_POSITIVE_ACTIVE_POWER = 0x00010400;
const uint32_t SMA_POSITIVE_REACTIVE_POWER = 0x00030400;
const uint32_t SMA_NEGATIVE_ACTIVE_POWER = 0x00020400;
const uint32_t SMA_NEGATIVE_REACTIVE_POWER = 0x00040400;
const uint32_t SMA_VERSION = 0x90000000;

// IDs to identify offsets in the SMA meter header
const uint8_t WLEN = 0xfa;
const uint8_t DSRC = 0xfb;
const uint8_t DTIM = 0xfc;

// Protocol-header for SMA energy meter packet
const uint8_t SMA_METER_HEADER[] = { 'S', 'M', 'A', 0,                         // Identifier
                             0x00, 0x04, 0x02, 0xa0, 0x00, 0x00, 0x00, 0x01,   // Group 1
                             WLEN, WLEN, 0x00, 0x10, 0x60, 0x69,               // Start of protocol 0x6069
                             0x01, 0x0e, DSRC, DSRC, DSRC, DSRC,               // Source address
                             DTIM, DTIM, DTIM, DTIM };                         // Timestamp

// Initial length of the payload (Protocol-ID + SRC + Time)
const int INITIAL_PAYLOAD_LENGTH = 12;

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------

// Buffer for serial reading
const int SML_PACKET_SIZE = 1000;
uint8_t smlPacket[SML_PACKET_SIZE];

// Buffer to store the energy-meter packet
const int METER_PACKET_SIZE = 1000;
uint8_t meterPacket[METER_PACKET_SIZE];

// Length of the energy meter packet header
uint16_t meterHeaderLength;

// Pointer to the data-length field in the energy meter packet
uint8_t *pMeterDataSize;

// Pointer to the time field in the energy meter packet
uint8_t *pMeterTime;

// UDP instance for sending packets
WiFiUDP Udp;

/**
* @brief Turn status led on
*/
void ledOn() {
   digitalWrite(LED_BUILTIN, LOW);
}

/**
* @brief Turn status led off
*/
void ledOff() {
   digitalWrite(LED_BUILTIN, HIGH);
}

/**
* @brief Blink function for status feedback
*/
void blink(int count, int onDuration, int offDuration, int delayAfter) {
   for (int i = 0; i < count; i++) {
      ledOn();
      delay(onDuration);
      ledOff();
      delay(offDuration);
   }
   delay(delayAfter);
}

/**
* @brief Store an U16 in big endian byte order
*/
uint8_t *storeU16BE(uint8_t *pPos, uint16_t value) {
   *(pPos++) = value >> 8;
   *(pPos++) = value & 0xff;
   return pPos;
}

/**
* @brief Store an U32 in big endian byte order
*/
uint8_t *storeU32BE(uint8_t *pPos, uint32_t value) {
   pPos = storeU16BE(pPos, value >> 16);
   return storeU16BE(pPos, value & 0xffff);
}

/**
* @brief Store an U64 in big endian byte order
*/
uint8_t *storeU64BE(uint8_t *pPos, uint64_t value) {
   pPos = storeU32BE(pPos, value >> 32);
   return storeU32BE(pPos, value & 0xffffffff);
}

/**
* @brief Find the offset of the given identifier
*/
uint8_t* offsetOf(uint8_t *pData, uint8_t identifier, int size) {
   for (int i = 0; i < size; ++i) {
      if (pData[i] == identifier) {
         return pData + i;
      }
   }
   Serial.print("Error: could not find offset: ");
   Serial.println(identifier);
   return 0;
}

/**
* @brief Initialize energy meter packet
*/
void initEmeterPacket(uint32_t serNo) {
   meterHeaderLength = sizeof(SMA_METER_HEADER);
   memcpy(meterPacket, SMA_METER_HEADER, meterHeaderLength);

   pMeterDataSize = offsetOf(meterPacket, WLEN, meterHeaderLength);
   pMeterTime = offsetOf(meterPacket, DTIM, meterHeaderLength);

   uint8_t *pSerNo = offsetOf(meterPacket, DSRC, meterHeaderLength);
   storeU32BE(pSerNo, serNo);
}

/**
* @brief Read the next value from a sml packet
* @note This Reader ignores lists tags. If a list occurs, it will
*       return the next value after the list.
*/
uint64_t getSmlValue(uint8_t *pPacket, int *pOffset) {
   int p = *pOffset;
   uint8_t type = pPacket[p] & SML_TAG_MASK;
   while (type == SML_LIST_TAG) {
      type = pPacket[++p] & SML_TAG_MASK;
   }
   uint8_t length = pPacket[p] & SML_LENGTH_MASK;
   ++p;
   uint64_t value = 0;
   for (int i = 1; i < length; ++i) {
      value = (value << 8) | pPacket[p++];
   }
   *pOffset = p;
   return value;
}

/**
* @brief Update the energy meter packet by analyzing the given sml packet
*/
uint16_t updateEmeterPacket(uint8_t *pSmlPacket, int smlLength) {
   uint8_t *pPacketPos = meterPacket + meterHeaderLength;
   storeU32BE(pMeterTime, millis());

   // Initial length of packet (ID + SN + TS)
   uint16_t length = INITIAL_PAYLOAD_LENGTH;

   int patternPos = 0;
   for (int i = 0; i < smlLength; ++i) {
      if (pSmlPacket[i] == SML_ID_PATTERN[patternPos]) {
         patternPos++;
         if (patternPos == 4) {
            // Get the SML values
            uint8_t index = pSmlPacket[++i];
            uint8_t type = pSmlPacket[++i];
            uint8_t tariff = pSmlPacket[++i];
            ++i; // Skip next value
            uint32_t status = (uint32_t)getSmlValue(pSmlPacket, &i);
            uint8_t timeType = (uint8_t)getSmlValue(pSmlPacket, &i);
            uint32_t time = (uint32_t)getSmlValue(pSmlPacket, &i);
            uint8_t unit = (uint8_t)getSmlValue(pSmlPacket, &i);
            int32_t scale = (int32_t)getSmlValue(pSmlPacket, &i);
            if (scale >= 0x80) {
               scale -= 256;
            }
            uint64_t value = getSmlValue(pSmlPacket, &i);
            patternPos = 0;

            // Store the value in the energy meter packet
            if ((scale >= SML_MIN_SCALE) && (scale <= SML_MAX_SCALE)) {
               // Calculate the index in the array of scale values
               int scaleIndex = scale - SML_MIN_SCALE;

               if (type == OBIS_INSTANTANEOUS_POWER_TYPE) {
                  int32_t scaleFactor = SCALE_FACTORS_POWER[scaleIndex];
                  int32_t intValue = (int32_t)value;
                  if (scaleFactor < 0) {
                     intValue /= (-scaleFactor);
                  }
                  else {
                     intValue *= scaleFactor;
                  }
                  switch (index) {
                  case OBIS_POSITIVE_ACTIVE_POWER:
                     pPacketPos = storeU32BE(pPacketPos, SMA_POSITIVE_ACTIVE_POWER);
                     pPacketPos = storeU32BE(pPacketPos, intValue);
                     pPacketPos = storeU32BE(pPacketPos, SMA_POSITIVE_REACTIVE_POWER);
                     pPacketPos = storeU32BE(pPacketPos, 0);
                     length += 16;
                     break;
                  case OBIS_NEGATIVE_ACTIVE_POWER:
                     pPacketPos = storeU32BE(pPacketPos, SMA_NEGATIVE_ACTIVE_POWER);
                     pPacketPos = storeU32BE(pPacketPos, intValue);
                     pPacketPos = storeU32BE(pPacketPos, SMA_NEGATIVE_REACTIVE_POWER);
                     pPacketPos = storeU32BE(pPacketPos, 0);
                     length += 16;
                     break;
                  case OBIS_SUM_ACTIVE_POWER:
                     pPacketPos = storeU32BE(pPacketPos, SMA_POSITIVE_ACTIVE_POWER);
                     pPacketPos = storeU32BE(pPacketPos, intValue >= 0 ? intValue : 0);
                     pPacketPos = storeU32BE(pPacketPos, SMA_POSITIVE_REACTIVE_POWER);
                     pPacketPos = storeU32BE(pPacketPos, 0);
                     pPacketPos = storeU32BE(pPacketPos, SMA_NEGATIVE_ACTIVE_POWER);
                     pPacketPos = storeU32BE(pPacketPos, intValue <= 0 ? -intValue : 0);
                     pPacketPos = storeU32BE(pPacketPos, SMA_NEGATIVE_REACTIVE_POWER);
                     pPacketPos = storeU32BE(pPacketPos, 0);
                     length += 32;
                  }
               }
               if (type == OBIS_ENERGY_TYPE) {
                  value *= SCALE_FACTORS_ENERGY[scaleIndex];
                  *(pPacketPos++) = 0;
                  *(pPacketPos++) = index;
                  *(pPacketPos++) = type;
                  *(pPacketPos++) = tariff;
                  pPacketPos = storeU64BE(pPacketPos, value);
                  length += 12;
               }
            }
            else {
               Serial.print("Error: Scale value out of bounds: ");
               Serial.println(scale);
            }
         }
      }
      else {
         patternPos = 0;
      }
   }

   // Store version
   pPacketPos = storeU32BE(pPacketPos, SMA_VERSION);
   pPacketPos = storeU32BE(pPacketPos, 0x01020452);
   length += 8;

   // Update length
   storeU16BE(pMeterDataSize, length);

   // Add end-tag
   storeU32BE(pPacketPos, 0);
   length += 4;

   return meterHeaderLength + length - INITIAL_PAYLOAD_LENGTH;
}

/**
* @brief Read next packet from the serial interface
*/
int readSerial() {
   // Wait until something is received
   Serial.print("W");
   int flashcount = 0;
   while (!Serial.available()) {
      flashcount++;
      if (flashcount == 490) {
         ledOn();
      }
      else if (flashcount > 500) {
         ledOff();
         flashcount = 0;
      }
      else {
         delay(5);  // wait 5 ms for new packets
      }
   }

   // We got some bytes. Read until next pause
   Serial.print("R");
   ledOn();
   Serial.setTimeout(500);
   int serialLen = Serial.readBytes(smlPacket, SML_PACKET_SIZE);
   ledOff();
   return serialLen;
}

/**
* @brief Setup the sketch
*/
void setup() {
   // Initialize the LED_BUILTIN pin as an output
   pinMode(LED_BUILTIN, OUTPUT);
   // Signal startup
   blink(1, 5000, 500, 1000); 
   // Open serial communications and wait for port to open      
   Serial.begin(9600);
   while (!Serial);

   Serial.println("ESP8266-D0 to SMA Energy Meter");
   Serial.print("Chip-ID: ");
   Serial.println(ESP.getChipId());
   Serial.print("Flash-chip-ID: ");
   Serial.println(ESP.getFlashChipId());
   Serial.print("MAC address: ");
   Serial.println(WiFi.macAddress());

   // Signal Serial OK
   blink(2, 100, 500, 2000);   

   Serial.print("Start WiFi to SSID: ");
   Serial.println(WIFI_SSID);
   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
   int retries = 5;
   while (WiFi.status() != WL_CONNECTED) {
      --retries;
      blink(3, 50, 100, 2000);
      if (retries <= 0) {
         Serial.println("Failed to init WiFi ... restart!");
         ESP.restart();
      }
   }

   Serial.print("Connected. IP:");
   Serial.println(WiFi.localIP());

   // Signal WiFi OK
   blink(4, 100, 500, 2000);

   if (SER_NO == 0) {
      initEmeterPacket(990000000 + ESP.getChipId());
   }
   else {
      initEmeterPacket(SER_NO);
   }

   if (USE_MULTICAST) {
      // Multicast IP address used for energy meter telegrams
      destinationAddress = IPAddress(239, 12, 255, 254);
   }

   Serial.println("Initialization done.");
}

/**
* @brief Main loop
*/
void loop() {
   Serial.print("_");

   int smlPacketLength;

   if (!USE_DEMO_DATA) {
      smlPacketLength = readSerial();
   }
   else {
      smlPacketLength = SML_TEST_PACKET_LENGTH;
      memcpy(smlPacket, SML_TEST_PACKET, smlPacketLength);
      delay(1000);
   }

   if (smlPacketLength <= SML_PACKET_SIZE) {
      uint32_t *pSmlHeader = (uint32_t*)smlPacket;
      if ((pSmlHeader[0] == SML_ESCAPE) && (pSmlHeader[1] == SML_VERSION1)) {
         Serial.print("S");

         if (USE_MULTICAST) {
            Udp.beginPacketMulticast(destinationAddress, DESTINATION_PORT, WiFi.localIP(), 1);
         }
         else {
            Udp.beginPacket(destinationAddress, DESTINATION_PORT);
         }

         if (SEND_EMETER_PACKET) {
            int meterPacketLength = updateEmeterPacket(smlPacket, smlPacketLength);
            Udp.write(meterPacket, meterPacketLength);
         }
         else {
            Udp.write(smlPacket, smlPacketLength);
         }

         // Send paket
         Udp.endPacket();
      }
      else {
         Serial.print("E");
      }
   }
   else {
      // Error
      blink(10, 100, 100, 2000);
   }
   Serial.println(".");
}
