/**
   ESP8266 SML to SMA energy meter converter

   This sketch reads SML telegrams from an infrared D0 interface, converts them to SMA energy-meter
   telegrams and sends them via UDP.

   Dependencies:
   esp8266 board, Version 2.7.4
   IotWebConf, Version 2.3.1
   PubSubClient, Version 2.8.0

   Flash-layout:
   4MB FS 1MB, OTA

   Configuration:
   This sketch provides a web-server for configuration.
   For more details, see the readme:
   https://github.com/jtuemmler/sml2emeter/blob/master/readme.adoc
*/

#include <IotWebConf.h>
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <PubSubClient.h>
#include <spi_flash.h>
#include <SoftwareSerial.h>
#include "util/sml_testpacket.h"
#include "smlstreamreader.h"
#include "smlparser.h"
#include "emeterpacket.h"
#include "counter.h"
#include "webconfparameter.h"

// ----------------------------------------------------------------------------
// Compile time settings
// ----------------------------------------------------------------------------

// Application version
const char VERSION[] = "Version 1.5.E";

// Controls whether to publish debugging-information regarding impulse detection
const int MIRROR_SERIAL_PIN = D2;

// Use demo data
//  Set to false, to read data from serial port or to
//  true: Use build-in demo data
const bool USE_DEMO_DATA = false;

// Time to wait for demo-data
const int TEST_PACKET_RECEIVE_TIME_MS = (SML_TEST_PACKET_LENGTH * 8 * 1000) / 9600;

// Default multicast address for energy meter packets
const IPAddress MCAST_ADDRESS = IPAddress(239, 12, 255, 254);

// Port used for energy meter packets
const uint16_t SMA_ENERGYMETER_PORT = 9522;

// PIN for pulse-counting
const int PULSE_INPUT_PIN = D1;

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
const char CONFIG_VERSION[] = "v2";

// When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//   password to buld an AP. (E.g. in case of lost password)
const int CONFIG_PIN = D3;

// Static part of the web-page
const char INDEX_HTML[] = "<!doctypehtml><meta charset=utf-8><meta content=\"width=device-width,initial-scale=1,user-scalable=no\"name=viewport><title>Energy meter</title><script>function u(a,t){var r=new XMLHttpRequest;r.onreadystatechange=function(){if(4==r.readyState&&200==r.status){var t=JSON.parse(r.responseText),e=\"\";for(var n in t)e=e.concat(\"<tr><th>{k}</th><td>{v}</td></tr>\".replace(\"{k}\",n).replace(\"{v}\",t[n]));document.getElementById(a).innerHTML='<table style=\"width:100%\">{d}</table>'.replace(\"{d}\",e)}},r.open(\"GET\",t,!0),r.send()}function r(){u(\"data\",\"data\")}function i(){r();self.setInterval(function(){r()},2e3)}window.onload=i()</script><style>div{padding:5px;font-size:1em}p{margin:.5em 0}body{text-align:center;font-family:verdana}td{padding:0}th{padding:5px;width:50%}td{padding:5px;width:50%}button{border:0;border-radius:.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;-webkit-transition-duration:.4s;transition-duration:.4s;cursor:pointer}button:hover{background-color:#0e70a4}</style><div style=text-align:left;display:inline-block;min-width:340px><div style=text-align:center><noscript>Please enable JavaScript<br></noscript><h2>Energy meter</h2></div><div id=data> </div><p><form action=config><button>Configuration</button></form><div style=text-align:right;font-size:11px><hr>{v}</div></div>";

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------

// Buffer for serial reading
const int SML_PACKET_SIZE = 1000;

// Reader for SML streams
SmlStreamReader smlStreamReader(SML_PACKET_SIZE);

// Parser for SML packets
SmlParser smlParser;

// Class for generating e-meter packets
EmeterPacket emeterPacket;

// Errors while reading packets from the serial interface
uint32_t readErrors = 0;

// Errors while reading packets from the serial interface
uint32_t mqttSendErrors = 0;

// Counter for failed wifi connection attempts
int failedWifiConnections = 0;

// Current state of the led when in connection-mode
bool ledState = false;

// Time to change the state of the led
unsigned long nextLedChange = 0;

// Timeout for pulse-counting. If set to 0, impulse counting is turned off.
volatile unsigned long pulseTimeoutMs = 0;

// Indicates wether the beginning of an impulse has been detected
volatile int isrArmed = 0;

// Indicates the last state of the impulse-pin (HIGH / LOW)
volatile int isrLastState = -2;

// Counted impulses
volatile unsigned long impulses = 0UL;

// Counted interrupts
volatile unsigned long interruptCount = 0UL;

// Time (ticks) of the last received impulse
volatile unsigned long lastPulseEventMs = 0UL;

// Persisted instance of the impulse counter
Counter impulseCounter;

// Factor for m3 calculation
float pulseFactor = 0.01;

#if DEBUG_IMPULSES > 0

#define MAX_EVENTS 20
volatile unsigned long eventTs[MAX_EVENTS];
volatile char eventVal[MAX_EVENTS];
volatile int eventPos = 0;

#endif

SoftwareSerial mirrorSerial;

// Destination addresses for sending meter packets
const int DEST_ADDRESSES_SIZE = 2;
IPAddress destAddresses[DEST_ADDRESSES_SIZE];
uint8_t numDestAddresses = 0;

// Destination ports
uint16_t ports[DEST_ADDRESSES_SIZE];

// UDP instance for sending packets
WiFiUDP Udp;

// DNS server instance
DNSServer dnsServer;

// Webserver instance
WebServer server(80);

// HTTP update server
HTTPUpdateServer httpUpdater;

// MQTT Client
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
String mqttTopic;
String mqttTopicImpulses;
int mqttPort = 0;
int mqttRetryCounter = 0;
unsigned long mqttLastPublishedImpulses = 0UL;

// IotWebConf instance
IotWebConf iotWebConf(THING_NAME, &dnsServer, &server, WIFI_INITIAL_AP_PASSWORD, CONFIG_VERSION);

// User-defined configuration values for IotWebConf
WebConfParameter separator1(iotWebConf, "SMA energy-meter configuration");
WebConfParameter destinationAddress1Param(iotWebConf, "Unicast address 1", "destinationAddress1", STRING_LEN);
WebConfParameter destinationAddress2Param(iotWebConf, "Unicast address 2", "destinationAddress2", STRING_LEN);
WebConfParameter portParam(iotWebConf, "Port (default 9522, 0 to turn off)", "port", NUMBER_LEN, "number", "9522", "min='0' max='65535' step='1'");
WebConfParameter serialNumberParam(iotWebConf, "Serial number", "serialNumber", NUMBER_LEN, "number", "", "min='0' max='999999999' step='1'");

WebConfParameter separator2(iotWebConf, "MQTT broker configuration");
WebConfParameter mqttBrockerAddressParam(iotWebConf, "Hostname", "mqttBrockerAddress", STRING_LEN);
WebConfParameter mqttPortParam(iotWebConf, "Port (default 1883, 0 to turn off)", "mqttPort", NUMBER_LEN, "number", "0", "min='0' max='65535' step='1'");

WebConfParameter separator3(iotWebConf, "Pulse counting");
WebConfParameter pulseTimeoutMsParam(iotWebConf, "Timeout for pulse-counter (ms)", "pulseTimeoutMs", NUMBER_LEN, "number", "0", "min='0' max='100000' step='1'");
WebConfParameter pulseFactorParam(iotWebConf, "Factor for pulse-counter", "pulseFactor", NUMBER_LEN, "number", "0.01", "min='0' max='100000' step='0.01'");

/**
   @brief Turn status led on
*/
void ledOn(bool overrideComState = false) {
   if ((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) || overrideComState) {
      digitalWrite(LED_BUILTIN, LOW);
   }
}

/**
   @brief Turn status led off
*/
void ledOff(bool overrideComState = false) {
   if (((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) && (nextLedChange == 0)) || overrideComState) {
      digitalWrite(LED_BUILTIN, HIGH);
   }
}

/**
   @brief Turn led for given time on
*/
void ledOnFor(unsigned int ledTimeoutMs) {
   if (iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) {
      ledOn();
      nextLedChange = millis() + ledTimeoutMs;
      if (nextLedChange == 0) {
         nextLedChange = 1;
      }
   }
}

/**
   @brief Signal connection state
*/
void signalConnectionState() {
   if (iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) {
      failedWifiConnections = 0;
      if ((nextLedChange > 0) && (millis() > nextLedChange)) {
         nextLedChange = 0;
         ledOff();
      }
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
}

/**
   @brief Store the detected number of impulses in flash.
*/
void storeImpulseCounter() {
   if (pulseTimeoutMs > 0) {
      uint32_t currentImpulses = 0;
      noInterrupts();
      currentImpulses = impulses;
      interrupts();
      while (currentImpulses > impulseCounter.get()) {
         Serial.print("s");
         impulseCounter.increment();
      }
   }
}

/**
   @brief Wait the given time in ms
*/
void delayMs(unsigned long delayMs) {
   unsigned long start = millis();
   while (millis() - start < delayMs) {
      iotWebConf.doLoop();
      signalConnectionState();
      if ((mqttPort > 0) && (iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE)) {
        mqttClient.loop();
      }      
      delay(1);
   }
   storeImpulseCounter();
}

/**
   @brief Handle interrupt from pulse-counter
*/
ICACHE_RAM_ATTR void handlePulseInterrupt() {
   // Check whether impulses should be detected.
   if (pulseTimeoutMs > 0) {
      // Check whether the state if the dection pin has been changed.
      int state = digitalRead(PULSE_INPUT_PIN);
      if (state == isrLastState) {
         return;
      }
      isrLastState = state;

      // State has changed ...
      unsigned long currentTimeMs = millis();

#if DEBUG_IMPULSES > 0
      if (eventPos < MAX_EVENTS) {
         eventTs[eventPos] = micros();
         eventVal[eventPos++] = state == LOW ? '0' : '1';
      }
#endif

      // If we changed from HIGH -> LOW, the beginning of an impulse has been detected.
      // Now wait until the signal is released ...
      if (state == LOW) {
         lastPulseEventMs = currentTimeMs;
         isrArmed = 1;
      }
      else if ((state == HIGH) && (isrArmed == 1)) {
         // Signal was released and we've detected the beginning before.
         isrArmed = 0;
         // Now check if the debounce-timeout has been elapsed. If so, count the impulse.
         if (((currentTimeMs - lastPulseEventMs) > pulseTimeoutMs) || (currentTimeMs < lastPulseEventMs)) {
            ledOnFor(2000);
            ++impulses;
            Serial.print("i");
         }
      }
   }
}

/**
   @brief Update the energy meter packet
*/
void updateEmeterPacket() {
   emeterPacket.begin(millis());

   // Store active and reactive power (convert from centi-W to deci-W)
   emeterPacket.addMeasurementValue(EmeterPacket::SMA_POSITIVE_ACTIVE_POWER, smlParser.getPowerInW() / 10);
   emeterPacket.addMeasurementValue(EmeterPacket::SMA_NEGATIVE_ACTIVE_POWER, smlParser.getPowerOutW() / 10);
   emeterPacket.addMeasurementValue(EmeterPacket::SMA_POSITIVE_REACTIVE_POWER, 0);
   emeterPacket.addMeasurementValue(EmeterPacket::SMA_NEGATIVE_REACTIVE_POWER, 0);

   // Store energy (convert from centi-Wh to Ws)
   emeterPacket.addCounterValue(EmeterPacket::SMA_POSITIVE_ENERGY, smlParser.getEnergyInWh() * 36UL);
   emeterPacket.addCounterValue(EmeterPacket::SMA_NEGATIVE_ENERGY, smlParser.getEnergyOutWh() * 36UL);

   emeterPacket.end();
}

/**
   @brief Read next packet from the serial interface
*/
void readSerial() {
   Serial.print("W");
   ledOff();
   bool receiving = false;
   int waitCount = 0;
   do {
      int data = Serial.read();
      if (data >= 0) {
         if (!receiving) {
            Serial.print("R");
            ledOnFor(500);
            receiving = true;
            waitCount = 0;
         }
         uint8_t dataByte = (uint8_t)data;
         if (MIRROR_SERIAL_PIN >= 0) {
            mirrorSerial.write(dataByte);
         }
         if (smlStreamReader.addData(&dataByte, 1) >= 0) {
            break;
         }
      }
      else {
         ++waitCount;
         if (waitCount == 195) {
            ledOn();
         }
         if (waitCount == 200) {
            Serial.print("T");
            ledOff();
            waitCount = 0;
            ++readErrors;
         }
         delayMs(10);
      }
   } while (true);
}

/**
   @brief Read test packet
*/
void readTestPacket() {
   Serial.print("W");
   ledOff();
   ledOnFor(1000 - TEST_PACKET_RECEIVE_TIME_MS);
   Serial.print("R");
   ledOn();
   delayMs(TEST_PACKET_RECEIVE_TIME_MS);
   smlStreamReader.addData(SML_TEST_PACKET, SML_TEST_PACKET_LENGTH);
}

/**
   @brief Handle web requests to "/" path.
*/
void handleRoot()
{
   // Let IotWebConf test and handle captive portal requests.
   if (iotWebConf.handleCaptivePortal()) {
      // Captive portal request were already served.
      return;
   }

   String page = INDEX_HTML;
   page.replace("{v}", VERSION);
   server.send(200, "text/html", page);
}

/**
   @brief Get current meter data as JSON data
   @param detailed Return detailed data if true
*/
String getCurrentDataAsJson(bool detailed = true) {
   String data = "{";
   if (smlParser.getParsedOk() > 0) {
      data += "\"PowerIn\":";
      data += smlParser.getPowerInW() / 100.0;
      data += ",\"EnergyIn\":";
      data += smlParser.getEnergyInWh() / 100.0;
      data += ",\"PowerOut\":";
      data += smlParser.getPowerOutW() / 100.0;
      data += ",\"EnergyOut\":";
      data += smlParser.getEnergyOutWh() / 100.0;
      if (detailed) {
         data += ",";
      }
   }
   if (detailed) {
      data += "\"Ok\":";
      data += (unsigned int)smlParser.getParsedOk();
      data += ",\"ReadErrors\":";
      data += (unsigned int)readErrors;
      data += ",\"ParseErrors\":";
      data += (unsigned int)(smlParser.getParseErrors() + smlStreamReader.getParseErrors());
      if (pulseTimeoutMs > 0) {
         data += ",\"Impulses\":";
         data += impulses;
         data += ",\"m^3\":";
         data += impulses * pulseFactor;
      }
      if (mqttPort > 0) {
         data += ",\"MqttClientState\":";
         data += mqttClient.state();
         data += ",\"MqttSendErrors\":";
         data += (unsigned int)mqttSendErrors;
      }
   }
   data += "}";

   return data;
}

/**
   @brief Return the current readings as json object
*/
void handleData() {
   server.send(200, "application/json", getCurrentDataAsJson());
}

/**
   @brief Check, whether a valid IP address is given
*/
bool checkIp(WebConfParameter &parameter) {
   IotWebConfParameter &iotWebConfParameter = *parameter.get();
   IPAddress ip;

   String arg = server.arg(iotWebConfParameter.getId());
   if (arg.length() > 0) {
      char ipAddress[STRING_LEN + 1] = { 0 };
      strncpy(ipAddress, arg.c_str(), STRING_LEN);
      char *pPortPos = strchr(ipAddress, ':');
      if (pPortPos != NULL) {
         *pPortPos = 0;
      }
      if (!ip.fromString(ipAddress)) {
         iotWebConfParameter.errorMessage = "IP address is not valid!";
         return false;
      }
   }
   return true;
}

/**
   @brief Validate input in the form
*/
bool formValidator() {
   Serial.println("Validating form.");
   bool valid = checkIp(destinationAddress1Param) && checkIp(destinationAddress2Param);

   return valid;
}

/**
   @brief Parse IP addresses and ports
 */
void parseDestinationAddress(const char *pDestinationAddressValue) {
   char destinationAddress[STRING_LEN + 1] = { 0 };
   strncpy(destinationAddress, pDestinationAddressValue, STRING_LEN);
   char *pPortPos = strchr(destinationAddress, ':');
   if (pPortPos == NULL) {
      ports[numDestAddresses] = portParam.getInt();
   }
   else {
      *pPortPos = 0;
      ports[numDestAddresses] = atoi(pPortPos + 1);
   }
   if (destAddresses[numDestAddresses].fromString(destinationAddress)) {
      ++numDestAddresses;
   }
}

/**
   @brief Process changed configuration
*/
void configSaved() {
   Serial.println("Configuration was updated.");
   numDestAddresses = 0;
   parseDestinationAddress(destinationAddress1Param.getText());
   parseDestinationAddress(destinationAddress2Param.getText());

   Serial.print("serNo: "); Serial.println(serialNumberParam.getInt());
   for (int i = 0; i < numDestAddresses; ++i) {
      Serial.print("Destination address: ");
      Serial.print(destAddresses[i].toString());
      Serial.print(", port: ");
      Serial.println(ports[i]);
   }

   emeterPacket.init(serialNumberParam.getInt());

   mqttClient.disconnect();
   mqttPort = mqttBrockerAddressParam.isEmpty() ? 0 : mqttPortParam.getInt();
   if (mqttPort > 0) {
      mqttTopic = String("/") + iotWebConf.getThingName() + String("/data");
      mqttTopicImpulses = String("/") + iotWebConf.getThingName() + String("/impulses");

      Serial.print("mqttTopic: "); Serial.println(mqttTopic);
      mqttClient.setServer(mqttBrockerAddressParam.getText(), mqttPort);
   }

   pulseTimeoutMs = pulseTimeoutMsParam.getInt();
   pulseFactor = pulseFactorParam.getFloat();

   if (pulseTimeoutMs > 0) {
      // Attach interrupt-handler
      Serial.print("Pulse-Pin: ");
      Serial.println(PULSE_INPUT_PIN);
      Serial.print("Interrupt: ");
      Serial.println(digitalPinToInterrupt(PULSE_INPUT_PIN));

      attachInterrupt(digitalPinToInterrupt(PULSE_INPUT_PIN), handlePulseInterrupt, CHANGE);
      //attachInterrupt(PULSE_INPUT_PIN, handlePulseInterrupt, FALLING);
   }
   else {
      detachInterrupt(digitalPinToInterrupt(PULSE_INPUT_PIN));
      //detachInterrupt(PULSE_INPUT_PIN);
   }
}

/**
   @brief Handle failed wifi-connections.
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
   @brief Print information about the flash-chip.
void flashInfo() {
   Serial.print("chip_size");
   Serial.println(flashchip->chip_size);

   Serial.print("block_size");
   Serial.println(flashchip->block_size);

   Serial.print("sector_size");
   Serial.println(flashchip->sector_size);

   Serial.print("page_size");
   Serial.println(flashchip->page_size);
}
*/

/**
   @brief Setup the sketch
*/
void setup() {
   // Initialize the LED_BUILTIN pin as an output
   pinMode(LED_BUILTIN, OUTPUT);
   pinMode(PULSE_INPUT_PIN, INPUT_PULLUP);

   // Open serial communications and wait for port to open
   Serial.begin(9600);
   while (!Serial);

   // Open software-serial to mirror SML data
   if (MIRROR_SERIAL_PIN >= 0) {
      mirrorSerial.begin(9600, SWSERIAL_8N1, -1, MIRROR_SERIAL_PIN, true);
   }

   Serial.println();
   Serial.print("ESP8266-D0 to SMA Energy Meter ");
   Serial.println(VERSION);
   Serial.print("Chip-ID: ");
   Serial.println(ESP.getChipId());
   Serial.print("Flash-chip-ID: ");
   Serial.println(ESP.getFlashChipId());
   Serial.print("MAC address: ");
   Serial.println(WiFi.macAddress());

   impulseCounter.init(1000, 4096);
   impulses = impulseCounter.get();

   serialNumberParam.setInt(990000000 + ESP.getChipId());
   portParam.setInt(SMA_ENERGYMETER_PORT);

   //iotWebConf.setConfigPin(CONFIG_PIN);
   iotWebConf.setConfigSavedCallback(&configSaved);
   iotWebConf.setFormValidator(&formValidator);
   iotWebConf.setupUpdateServer(&httpUpdater, "/update");
   iotWebConf.getApTimeoutParameter()->visible = false;
   iotWebConf.setWifiConnectionFailedHandler([]() {
      return handleWifiConnectionFailed();
   });

   // Initializing the configuration.
   iotWebConf.init();
   configSaved();

   // Set up required URL handlers on the web server.
   server.on("/", []() {
      handleRoot();
   });
   server.on("/data", []() {
      handleData();
   });
   server.on("/config", []() {
      iotWebConf.handleConfig();
   });
   server.onNotFound([]() {
      iotWebConf.handleNotFound();
   });

   Serial.println("Initialization done.");
}

/**
   @brief Publish data for emeter-protocol
*/
void publishEmeter() {
   if (ports[0] > 0) {
      updateEmeterPacket();
      // We use a do..while loop here to force at least one execution of the loop-body.
      int i = 0;
      do {
         Serial.print("S");
         if (numDestAddresses == 0) {
            Udp.beginPacketMulticast(MCAST_ADDRESS, ports[0], WiFi.localIP(), 1);                     
         }
         else {
            Udp.beginPacket(destAddresses[i], ports[i]);           
         }

         if (ports[i] == SMA_ENERGYMETER_PORT) {
            Udp.write(emeterPacket.getData(), emeterPacket.getLength());
         }
         else {
            Udp.write(smlStreamReader.getData(), smlStreamReader.getLength());
         }

         Udp.endPacket();      
      } while (++i < numDestAddresses);
   }
}

/**
   @brief Publish data to mqtt broker
*/
void publishMqtt() {
   if ((mqttPort == 0) || (iotWebConf.getState() != IOTWEBCONF_STATE_ONLINE)) {
      return;
   }

   Serial.print("M");

   if (!mqttClient.connected()) {
      if (--mqttRetryCounter > 0) {
         return;
      }
      mqttRetryCounter = 60;
      if (!mqttClient.connect(iotWebConf.getThingName())) {
         Serial.print("F");
         Serial.print(mqttClient.state());
         ++mqttSendErrors;
         return;
      }
      Serial.print("C");
   }

   mqttClient.loop();
   if (mqttClient.publish(mqttTopic.c_str(), getCurrentDataAsJson(false).c_str())) {
      Serial.print("S");
   }
   else {
      Serial.print("E");
      Serial.print(mqttClient.state());
      ++mqttSendErrors;
   }

   unsigned long currentTimeMs = millis();

#if DEBUG_IMPULSES > 0
   noInterrupts();
   unsigned long myTs[MAX_EVENTS];
   char myVal[MAX_EVENTS];
   int myEventPos = eventPos;
   for (int i = 0; i < myEventPos; ++i) {
      myTs[i] = eventTs[i];
      myVal[i] = eventVal[i];
   }
   eventPos = 0;
   interrupts();

   for (int i = 0; i < myEventPos; ++i) {
      String eventStr;
      eventStr += eventVal[i];
      eventStr += ":";
      eventStr += eventTs[i];
      //Serial.print(eventStr);
      mqttClient.publish("debug/interrupt", eventStr.c_str());
   }
#endif

   if (pulseTimeoutMs > 0) {
      noInterrupts();
      unsigned long mqttImpulses = impulses;
      unsigned long mqttInterruptCount = interruptCount;
      unsigned long mqttLastPulseEventMs = lastPulseEventMs;
      interrupts();

      if (mqttLastPublishedImpulses != mqttImpulses) {
         char buffer[100];
         sprintf(buffer, "{\"ts\":%lu, \"ets\":%lu, \"imp\":%lu, \"m3\":%g, \"intr\":%lu}", currentTimeMs, mqttLastPulseEventMs, mqttImpulses, mqttImpulses * pulseFactor, mqttInterruptCount);

         if (mqttClient.publish(mqttTopicImpulses.c_str(), buffer)) {
            mqttLastPublishedImpulses = mqttImpulses;
            Serial.print("I");
         }
         else {
            Serial.print("E");
            Serial.print(mqttClient.state());
         }
      }
   }
}

/**
   @brief Main loop
*/
void loop() {
   Serial.print("_");

   // Read the next packet
   if (!USE_DEMO_DATA) {
      readSerial();
   }
   else {
      readTestPacket();
   }

   // Send the packet
   if (smlParser.parsePacket(smlStreamReader.getData(), smlStreamReader.getLength())) {
      publishEmeter();
      publishMqtt();
   }
   else {
      Serial.print("E");
   }

   Serial.println(".");
}
