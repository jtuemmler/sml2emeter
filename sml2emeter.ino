/**
* ESP8266 SML to SMA energy meter converter
*
* ﻿This sketch reads SML telegrams from an infrared D0 interface, converts them to SMA energy-meter
* telegrams and send it via UDP.
*
* Dependencies:
* IotWebConf, Version 2.3.0
* PubSubClient, Version 2.7.0
*
* Configuration:
* This sketch provides a web-server for configuration.
* For more details, see the readme:
* https://github.com/jtuemmler/sml2emeter/blob/master/readme.adoc
*/

#include <IotWebConf.h>
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <PubSubClient.h>
#include "util/sml_testpacket.h"
#include "smlstreamreader.h"
#include "smlparser.h"
#include "emeterpacket.h"

// ----------------------------------------------------------------------------
// Compile time settings
// ----------------------------------------------------------------------------

// Application version
const char VERSION[] = "Version 1.4";

// Use demo data
//  Set to false, to read data from serial port or to
//  true: Use build-in demo data
const bool USE_DEMO_DATA = false;

// Time to wait for demo-data
const int TEST_PACKET_RECEIVE_TIME_MS = (SML_TEST_PACKET_LENGTH * 8 * 1000) / 9600;

// Default multicast address for energy meter packets
const IPAddress MCAST_ADDRESS = IPAddress(239, 12, 255, 254);

// Port used for energy meter packets
const uint16_t DESTINATION_PORT = 9522;

// Debugging (set SML_PORT = 0 to disable)
const IPAddress SML_ADDRESS = IPAddress(192, 168, 2, 100);
const uint16_t SML_PORT = 0;

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
const int CONFIG_PIN = D2;

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

// MQTT Client
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
String mqttTopic;
int mqttPort = 0;
int mqttRetryCounter = 0;

// IotWebConf instance
IotWebConf iotWebConf(THING_NAME, &dnsServer, &server, WIFI_INITIAL_AP_PASSWORD, CONFIG_VERSION);

// User-defined configuration values for IotWebConf
char destinationAddress1Value[STRING_LEN] = "";
char destinationAddress2Value[STRING_LEN] = "";
char serialNumberValue[NUMBER_LEN] = "";
char portValue[NUMBER_LEN] = "";
char mqttBrockerAddressValue[NUMBER_LEN] = "";
char mqttPortValue[NUMBER_LEN] = "0";

IotWebConfSeparator separator1("Meter configuration");
IotWebConfParameter serialNumberParam("Serial number", "serialNumber", serialNumberValue, NUMBER_LEN, "number", serialNumberValue, serialNumberValue, "min='0' max='999999999' step='1'");
IotWebConfParameter destinationAddress1Param("Unicast address 1", "destinationAddress1", destinationAddress1Value, STRING_LEN, "");
IotWebConfParameter destinationAddress2Param("Unicast address 2", "destinationAddress2", destinationAddress2Value, STRING_LEN, "");
IotWebConfParameter portParam("Port (default 9522)", "port", portValue, NUMBER_LEN, "number", portValue, portValue, "min='0' max='65535' step='1'");

IotWebConfSeparator separator2("MQTT broker configuration");
IotWebConfParameter mqttBrockerAddressParam("Hostname", "mqttBrockerAddress", mqttBrockerAddressValue, STRING_LEN, "");
IotWebConfParameter mqttPortParam("Port (default 1883)", "mqttPort", mqttPortValue, NUMBER_LEN, "number", mqttPortValue, mqttPortValue, "min='0' max='65535' step='1'");

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
* @brief Update the energy meter packet
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
* @brief Read next packet from the serial interface
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
            Serial.print("r");
            ledOn();
            receiving = true;
         }
         uint8_t byte = (uint8_t)data;
         if (smlStreamReader.addData(&byte, 1) >= 0) {
            break;
         }
      }
      else {
         ++waitCount;
         if (waitCount == 195) {
            ledOn();
         }
         if (waitCount == 200) {
            ledOff();
            waitCount = 0;
            ++readErrors;
         }
         delayMs(10);
      }
   } while (true);
   ledOff();
}

/**
* @brief Read test packet
*/
void readTestPacket() {
   Serial.print("w");
   ledOff();
   delayMs(1000 - TEST_PACKET_RECEIVE_TIME_MS);
   Serial.print("r");
   ledOn();
   delay(TEST_PACKET_RECEIVE_TIME_MS);
   smlStreamReader.addData(SML_TEST_PACKET, SML_TEST_PACKET_LENGTH);
   ledOff();
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
   page.replace("{v}", VERSION);
   server.send(200, "text/html", page);
}

/**
 * @brief Get current meter data as JSON data
 * @param detailed Return detailed data if true
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
      data += (unsigned int)smlParser.getParseErrors();
   }
   data += "}";

   return data;
}

/**
 * @brief Return the current readings as json object
 */
void handleData() {
   server.send(200, "application/json", getCurrentDataAsJson());
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

   emeterPacket.init(atoi(serialNumberValue));

   mqttClient.disconnect();
   mqttPort = mqttBrockerAddressValue[0] == 0 ? 0 : atoi(mqttPortValue);
   if (mqttPort > 0) {
      mqttTopic = "/";
      mqttTopic += iotWebConf.getThingName();
      mqttTopic += "/data";
      Serial.print("mqttTopic: "); Serial.println(mqttTopic);
      mqttClient.setServer(mqttBrockerAddressValue, mqttPort);
   }
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
   iotWebConf.addParameter(&separator2);
   iotWebConf.addParameter(&mqttBrockerAddressParam);
   iotWebConf.addParameter(&mqttPortParam);
   iotWebConf.setConfigSavedCallback(&configSaved);
   iotWebConf.setFormValidator(&formValidator);
   iotWebConf.setupUpdateServer(&httpUpdater, "/update");
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
 * @brief Publish data for emeter-protocol
 * @param smlPacketLength length of the SML packet
 */
void publishEmeter() {
   if (port > 0) {
      updateEmeterPacket();
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
            Udp.write(emeterPacket.getData(), emeterPacket.getLength());
         }
         else {
            Udp.write(smlStreamReader.getData(), smlStreamReader.getLength());
         }

         // Send paket
         Udp.endPacket();
         ++i;
      } while (i < numDestAddresses);
   }
}

/**
 * @brief Publish data to mqtt broker
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
   }
}

/**
* @brief Main loop
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
   
   // Debugging
   if (SML_PORT > 0) {
      Udp.beginPacket(SML_ADDRESS, SML_PORT);
      Udp.write(smlStreamReader.getData(), smlStreamReader.getLength());
      Udp.endPacket();
   }
   Serial.println(".");
}
