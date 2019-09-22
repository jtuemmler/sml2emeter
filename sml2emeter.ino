/**
* ESP8266 SML to SMA energy meter converter
*
* ﻿This sketch may be used to read SML telegrams from an infrared D0 interface, convert it to SMA energy-meter 
* telegrams and send it via UDP.
* 
* Dependencies:
* IotWebConf, Version 2.3.0
*
* Configuration:
* This sketch provides a web-server for configuration.
* For more details, see the readme:
* https://github.com/jtuemmler/sml2emeter/blob/master/readme.adoc
*/

#include <IotWebConf.h>
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include "util/sml_testpacket.h"
#include "smlparser.h"

// ----------------------------------------------------------------------------
// Compile time settings
// ----------------------------------------------------------------------------

// Application version
const char VERSION[] = "Version 1.2";

// Timeout for reading SML packets
const int SERIAL_TIMEOUT_MS = 100;

// Use demo data
//  Set to false, to read data from serial port or to
//  true: Use build-in demo data
const bool USE_DEMO_DATA = false;

// Time to wait for demo-data
const int TEST_PACKET_RECEIVE_TIME_MS = SERIAL_TIMEOUT_MS + SML_TEST_PACKET_LENGTH / 12;

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
const uint32_t SMA_POSITIVE_ENERGY = 0x00010800;
const uint32_t SMA_NEGATIVE_ENERGY = 0x00020800;
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

// Default multicast address for energy meter packets
const IPAddress MCAST_ADDRESS = IPAddress(239, 12, 255, 254);

// Port used for energy meter packets
const uint16_t DESTINATION_PORT = 9522;

// ----------------------------------------------------------------------------
// Constants for IotWebConf
// ----------------------------------------------------------------------------

// Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char THING_NAME[] = "sml2emeter";

// Initial password to connect to the Thing, when it creates an own Access Point.
const char WIFI_INITIAL_AP_PASSWORD[] = "sml2emeter";

const int STRING_LEN = 128;
const int NUMBER_LEN = 32;

// Configuration specific key. The value should be modified if config structure was changed.
const char CONFIG_VERSION[] = "v1";

// When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//   password to buld an AP. (E.g. in case of lost password)
const int CONFIG_PIN = D2;

const char INDEX_HTML[] = 
  "<!doctypehtml><meta charset=utf-8><meta content=\"width=device-width,initial-scale"
  "=1,user-scalable=no\"name=viewport><title>Energy meter</title><script>function u(a"
  ",t){var r=new XMLHttpRequest;r.onreadystatechange=function(){if(4==r.readyState&&2"
  "00==r.status){var t=JSON.parse(r.responseText),e=\"\";for(var n in t)e=e.concat(\""
  "<tr><th>{k}</th><td>{v}</td></tr>\".replace(\"{k}\",n).replace(\"{v}\",t[n]));docu"
  "ment.getElementById(a).innerHTML='<table style=\"width:100%\">{d}</table>'.replace"
  "(\"{d}\",e)}},r.open(\"GET\",t,!0),r.send()}function r(){u(\"data\",\"data\")}func"
  "tion i(){r();self.setInterval(function(){r()},2e3)}window.onload=i()</script><styl"
  "e>div{padding:5px;font-size:1em}p{margin:.5em 0}body{text-align:center;font-family"
  ":verdana}td{padding:0}th{padding:5px;width:50%}td{padding:5px;width:50%}button{bor"
  "der:0;border-radius:.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;f"
  "ont-size:1.2rem;width:100%;-webkit-transition-duration:.4s;transition-duration:.4s"
  ";cursor:pointer}button:hover{background-color:#0e70a4}</style><div style=text-alig"
  "n:left;display:inline-block;min-width:340px><div style=text-align:center><noscript"
  ">Please enable JavaScript<br></noscript><h2>Energy meter</h2></div><div id=data> <"
  "/div><p><form action=config><button>Configuration</button></form><div style=text-a"
  "lign:right;font-size:11px><hr>{v}</div></div>";

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

// Parser for SML packets
SmlParser smlParser;

// Errors while reading packets from the serial interface
uint32_t readErrors = 0;

// Counter for failed wifi connection attempts
int failedWifiConnections = 0;

// Current state of the led when in connection-mode
bool ledState = false;

// Time to change the state of the led
unsigned long nextLedChange = 0;

// Destination addresses for sending meter packets
const int DEST_ADRESSES_SIZE = 2;
IPAddress destAddresses[DEST_ADRESSES_SIZE];
uint8_t numDestAddresses = 0;

// Destination port
uint16_t port = DESTINATION_PORT;

// UDP instance for sending packets
WiFiUDP Udp;

// DNS server instance
DNSServer dnsServer;

// Webserver instance
WebServer server(80);

// HTTP update server
HTTPUpdateServer httpUpdater;

// IotWebConf instance
IotWebConf iotWebConf(THING_NAME, &dnsServer, &server, WIFI_INITIAL_AP_PASSWORD, CONFIG_VERSION);

// User-defined configuration values for IotWebConf
char destinationAddress1Value[STRING_LEN] = "";
char destinationAddress2Value[STRING_LEN] = "";
char serialNumberValue[NUMBER_LEN] = "";
char portValue[NUMBER_LEN] = "";

IotWebConfSeparator separator1("Meter configuration");
IotWebConfParameter serialNumberParam("Serial number","serialNumber",serialNumberValue,NUMBER_LEN,"number",serialNumberValue,serialNumberValue,"min='0' max='999999999' step='1'");
IotWebConfParameter destinationAddress1Param("Unicast address 1","destinationAddress1",destinationAddress1Value,STRING_LEN,"");
IotWebConfParameter destinationAddress2Param("Unicast address 2","destinationAddress2",destinationAddress2Value,STRING_LEN,"");
IotWebConfParameter portParam("Port","port",portValue,NUMBER_LEN,"number",portValue,portValue,"min='0' max='65535' step='1'");

/**
* @brief Turn status led on
*/
void ledOn(bool overrideComState = false) {
   if ((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) || overrideComState) {
      digitalWrite(LED_BUILTIN, LOW);
   }
}

/**
* @brief Turn status led off
*/
void ledOff(bool overrideComState = false) {
   if ((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) || overrideComState) {
      digitalWrite(LED_BUILTIN, HIGH);
   }
}

/**
 * @brief Signal connection state
 */
void signalConnectionState() {
   if (iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) {
      failedWifiConnections = 0;
      return;
   }
  
   if (millis() > nextLedChange) {
      ledState = !ledState;
      if (ledState) {
         nextLedChange = iotWebConf.getState() * 250;
         ledOn(true);
      }
      else {
         nextLedChange = 100;
         ledOff(true);
      }
      nextLedChange += millis();
   }
   iotWebConf.doLoop();
}

/**
 * @brief Wait the given time in ms
 */
void delayMs(unsigned long delayMs) {
   unsigned long start = millis();
   while (millis() - start < delayMs) {
      iotWebConf.doLoop();
      signalConnectionState();
      delay(1); 
   }
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
* @brief Update the energy meter packet
*/
uint16_t updateEmeterPacket() {
   uint8_t *pPacketPos = meterPacket + meterHeaderLength;
   storeU32BE(pMeterTime, millis());

   // Initial length of packet (ID + SN + TS)
   uint16_t length = INITIAL_PAYLOAD_LENGTH;

   // Store active and reactive power (convert from centi-W to deci-W)
   pPacketPos = storeU32BE(pPacketPos, SMA_POSITIVE_ACTIVE_POWER);
   pPacketPos = storeU32BE(pPacketPos, smlParser.getPowerInW() / 10);
   pPacketPos = storeU32BE(pPacketPos, SMA_NEGATIVE_ACTIVE_POWER);
   pPacketPos = storeU32BE(pPacketPos, smlParser.getPowerOutW() / 10);
   pPacketPos = storeU32BE(pPacketPos, SMA_POSITIVE_REACTIVE_POWER);
   pPacketPos = storeU32BE(pPacketPos, 0);
   pPacketPos = storeU32BE(pPacketPos, SMA_NEGATIVE_REACTIVE_POWER);
   pPacketPos = storeU32BE(pPacketPos, 0);
   length += 32;

   // Store energy (convert from centi-Wh to Ws)
   pPacketPos = storeU32BE(pPacketPos, SMA_POSITIVE_ENERGY);
   pPacketPos = storeU64BE(pPacketPos, smlParser.getEnergyInWh() * 36UL);
   pPacketPos = storeU32BE(pPacketPos, SMA_NEGATIVE_ENERGY);
   pPacketPos = storeU64BE(pPacketPos, smlParser.getEnergyOutWh() * 36UL);
   length += 24;

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
   ledOff();
   int flashcount = 0;
   while (!Serial.available()) {
      flashcount++;
      if (flashcount == 390) {
         ledOn();
      }
      else if (flashcount > 400) {
         ledOff();
         flashcount = 0;
         ++readErrors;
      }
      delayMs(5);
   }

   // We got some bytes. Read until next pause
   Serial.print("R");
   ledOn();
   Serial.setTimeout(SERIAL_TIMEOUT_MS);
   int serialLen = Serial.readBytes(smlPacket, SML_PACKET_SIZE);
   ledOff();
   return serialLen;
}

/**
* @brief Read test packet
*/
int readTestPacket() {
    Serial.print("w");
    ledOff();   
    for (int i = 0; i < 1000 - TEST_PACKET_RECEIVE_TIME_MS; i += 5) {
      delayMs(5);
    }
    Serial.print("r");
    ledOn();    
    delay(TEST_PACKET_RECEIVE_TIME_MS);
    memcpy(smlPacket, SML_TEST_PACKET, SML_TEST_PACKET_LENGTH);
    ledOff();
    return SML_TEST_PACKET_LENGTH;
}

/**
 * @brief Handle web requests to "/" path.
 */
void handleRoot()
{
    // Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal()) {
        // Captive portal request were already served.
        return;
    }

    String page = INDEX_HTML;
    page.replace("{v}",VERSION);
    server.send(200, "text/html", page);
}

/**
 * @brief Return the current readings as json object
 */
void handleData() {
    String data = "{";
    if (smlParser.getParsedOk() > 0) {
       data += "\"PowerIn\" : ";
       data += smlParser.getPowerInW() / 100.0;
       data += ",\"EnergyIn\" : ";
       data += smlParser.getEnergyInWh() / 100.0;
       data += ",\"PowerOut\" : ";
       data += smlParser.getPowerOutW() / 100.0;
       data += ",\"EnergyOut\" : ";
       data += smlParser.getEnergyOutWh() / 100.0;
       data += ",\"Ok\" : ";
       data += (unsigned int)smlParser.getParsedOk();
       data += ",\"Errors\" : ";
       data += (unsigned int)smlParser.getParseErrors() + readErrors;
    }
    data += "}";

    server.send(200, "application/json", data);
}

/**
 * @brief Check, whether a valid IP address is given
 */
bool checkIp(IotWebConfParameter &parameter) {
   IPAddress ip;

   String arg = server.arg(parameter.getId());
   if (arg.length() > 0) {
      if (!ip.fromString(arg)) {
         parameter.errorMessage = "IP address is not valid!";
         return false;
      }
   }
   return true;
}

/**
 * @brief Validate input in the form
 */
bool formValidator() {
   Serial.println("Validating form.");
   bool valid = checkIp(destinationAddress1Param) && checkIp(destinationAddress2Param);

   return valid;
}

/**
 * @brief Process changed configuration
 */
void configSaved() {
   Serial.println("Configuration was updated.");
   port = atoi(portValue);
   numDestAddresses = 0;
   if (destAddresses[numDestAddresses].fromString(destinationAddress1Value)) {
     ++numDestAddresses;
   }
   if (destAddresses[numDestAddresses].fromString(destinationAddress2Value)) {
     ++numDestAddresses;
   }

   Serial.print("serNo: "); Serial.println(serialNumberValue);
   Serial.print("port: "); Serial.println(port);
   Serial.print("numDestAddresses: "); Serial.println(numDestAddresses);

   initEmeterPacket(atoi(serialNumberValue));
}

/**
 * Handle faild wifi-connections.
 */
IotWebConfWifiAuthInfo* handleWifiConnectionFailed() {
   ++failedWifiConnections;
   if (failedWifiConnections >= 2) {
      Serial.println("Failed to init WiFi ... restart!");
      ESP.restart();
   }
   return NULL;
}

/**
* @brief Setup the sketch
*/
void setup() {
   // Initialize the LED_BUILTIN pin as an output
   pinMode(LED_BUILTIN, OUTPUT);
  
   // Open serial communications and wait for port to open      
   Serial.begin(9600);
   while (!Serial);

   Serial.println();
   Serial.print("ESP8266-D0 to SMA Energy Meter ");
   Serial.println(VERSION);
   Serial.print("Chip-ID: ");
   Serial.println(ESP.getChipId());
   Serial.print("Flash-chip-ID: ");
   Serial.println(ESP.getFlashChipId());
   Serial.print("MAC address: ");
   Serial.println(WiFi.macAddress());

   itoa(990000000 + ESP.getChipId(), serialNumberValue, 10);
   itoa(DESTINATION_PORT, portValue, 10);
   
   //iotWebConf.setConfigPin(CONFIG_PIN);
   iotWebConf.addParameter(&separator1);
   iotWebConf.addParameter(&destinationAddress1Param);
   iotWebConf.addParameter(&destinationAddress2Param);
   iotWebConf.addParameter(&portParam);
   iotWebConf.addParameter(&serialNumberParam);
   iotWebConf.setConfigSavedCallback(&configSaved);
   iotWebConf.setFormValidator(&formValidator);
   iotWebConf.setupUpdateServer(&httpUpdater,"/update");
   iotWebConf.getApTimeoutParameter()->visible = false;
   iotWebConf.setWifiConnectionFailedHandler([]() { return handleWifiConnectionFailed(); });

   // Initializing the configuration.
   iotWebConf.init();
   configSaved();

   // Set up required URL handlers on the web server.
   server.on("/", []() { handleRoot(); });
   server.on("/data", []() { handleData(); });
   server.on("/config", []() { iotWebConf.handleConfig(); });
   server.onNotFound([]() { iotWebConf.handleNotFound(); });

   Serial.println("Initialization done.");
}

/**
* @brief Main loop
*/
void loop() {
   Serial.print("_");

   // Read the next packet
   int smlPacketLength;
   if (!USE_DEMO_DATA) {
      smlPacketLength = readSerial();
   }
   else {
      smlPacketLength = readTestPacket();
   } 

   // Send the packet if a valid telegram was received
   if (smlPacketLength <= SML_PACKET_SIZE) {
      if (smlParser.parsePacket(smlPacket, smlPacketLength)) {
         if (port > 0) {
            int meterPacketLength = updateEmeterPacket();
            int i = 0;
            do {
               Serial.print("S");
               if (numDestAddresses == 0) {
                  Udp.beginPacketMulticast(MCAST_ADDRESS, port, WiFi.localIP(), 1);
               }
               else {
                  Udp.beginPacket(destAddresses[i], port);
               }

               if (port == DESTINATION_PORT) {
                  Udp.write(meterPacket, meterPacketLength);
               }
               else {
                  Udp.write(smlPacket, smlPacketLength);
               }

               // Send paket
               Udp.endPacket();
               ++i;
            } while (i < numDestAddresses);
         }
      }
      else {
         Serial.print("E");
      }
   }
   else {
      // Overflow error
      Serial.print("O");
      ++readErrors;
   }
   Serial.println(".");
}
