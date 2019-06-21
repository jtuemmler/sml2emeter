// Set SSID and PASSWORD for WiFi connection
const char WIFI_SSID[] = "MY_WIFI_SSID";
const char WIFI_PASSWORD[] = "MY_WIFI_PASSWORD";

// Set multicast mode.
//  If set to true, multicast messages will be used. Otherwise packets are send to
//  a fixed IP address defined below. Please note that sending an UDP packet to a 
//  fixed IP address performs better in wifi networks than sending multicast messages.
const bool USE_MULTICAST = true;

// Destination address for the packets.
//  (Only used, when multicast is set to false)
IPAddress destinationAddress(192, 168, 1, 100);

// Port used for energy meter packets
const uint16_t DESTINATION_PORT = 9522;

// Set serial number
//  This serial number will be used to identify this device.
//  If set to 0, the serial number will be calculated using the chip-id.
#ifdef EMETER_SERNO
const uint32_t SER_NO = EMETER_SERNO;
#else
const uint32_t SER_NO = 0;
#endif

// Use demo data
//  Set to false, to read data from serial port or to
//  true: Use build-in demo data
const bool USE_DEMO_DATA = false;

// Send SMA energy meter packet
//  If set to true: send sma energy meter packet
//  if set to false: send received sml packet (e.g. for debugging)
const bool SEND_EMETER_PACKET = true;
