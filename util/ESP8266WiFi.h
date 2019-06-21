// ----------------------------------------------------------------------------
// Wifi-stubs to let the sketch compile and run
// on a PC running windows, linux or macos
// ----------------------------------------------------------------------------

#ifndef ESP8266WIFI_H
#define ESP8266WIFI_H

#include "arduino_stubs.h"

const int WL_CONNECTED = 1;

class WifiInstance {
public:
   WifiInstance() {};
   void begin(const char *pSsid, const char *pPassword) {};
   IPAddress localIP() { return IPAddress(127, 0, 0, 1); }
   const char *macAddress() { return "aa:bb:cc:dd:ee:ff"; }
   int status() { return WL_CONNECTED; }
};

WifiInstance WiFi;

#endif
