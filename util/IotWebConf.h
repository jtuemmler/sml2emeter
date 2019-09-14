// ----------------------------------------------------------------------------
// IotWebConf-stubs to let the sketch compile and run
// on a PC running windows, linux or macos
// ----------------------------------------------------------------------------
#ifndef IOTWEBCONFIG_H
#define IOTWEBCONFIG_H

static const byte IOTWEBCONF_STATE_BOOT = 0;
static const byte IOTWEBCONF_STATE_NOT_CONFIGURED = 1;
static const byte IOTWEBCONF_STATE_AP_MODE = 2;
static const byte IOTWEBCONF_STATE_CONNECTING = 3;
static const byte IOTWEBCONF_STATE_ONLINE = 4;

#include <functional>

static const int D2 = 2;

class DNSServer {
public:
   DNSServer() {}
};

class WebServer {
public:
   WebServer(int port) {}
   void send(int code, const char *pResourceType, String &page) {}
   String arg(const char* id) { return String(); }
   void on(const char *pRessource, std::function<void()> func) {}
   void onNotFound(std::function<void()> func) {}
};

class HTTPUpdateServer {
public:
   HTTPUpdateServer() {}
};

class IotWebConfParameter {
public:
   IotWebConfParameter() {}
   IotWebConfParameter(
         const char* label, const char* id, char* valueBuffer, int length,
         const char* type = "text", const char* placeholder = NULL,
         const char* defaultValue = NULL, const char* customHtml = NULL,
         bool visible = true) {
      this->visible = visible;
   }
   IotWebConfParameter(
         const char* id, char* valueBuffer, int length, const char* customHtml,
         const char* type = "text") {}

   const char *getId() { return "Id"; }
   bool visible;
   const char *errorMessage;
};

class IotWebConfSeparator : public IotWebConfParameter {
public:
   IotWebConfSeparator(const char *pCaption) {}
};

class IotWebConfWifiAuthInfo {
};

class IotWebConf {
public:
   IotWebConf(const char *pThingName, const DNSServer *pDnsServer, const WebServer *pWebServer, const char *pApPassword, const char *pVersion) {}
   void setConfigPin(int configPin) {}
   void setStatusPin(int statusPin) {}
   void setupUpdateServer(
         HTTPUpdateServer* updateServer, const char* updatePath = "/firmware") {}
   bool init() { return true; }
   void doLoop() {}
   bool handleCaptivePortal() { return true; }
   void handleConfig() {}
   void handleNotFound() {}
   void setWifiConnectionCallback(std::function<void()> func) {}
   void setConfigSavedCallback(std::function<void()> func) {}
   void setFormValidator(std::function<bool()> func) {}
   void setApConnectionHandler(
         std::function<bool(const char* apName, const char* password)> func) {}
   void setWifiConnectionHandler(
         std::function<void(const char* ssid, const char* password)> func) {}
   void setWifiConnectionFailedHandler( std::function<IotWebConfWifiAuthInfo*()> func ) {}
   bool addParameter(IotWebConfParameter* parameter) { return true; }
   const char* getThingName() { return "Thing"; }
   void delay(unsigned long millis) { delay(millis); }
   void setWifiConnectionTimeoutMs(unsigned long millis) {}
   void blink(unsigned long repeatMs, byte dutyCyclePercent) {}
   void fineBlink(unsigned long onMs, unsigned long offMs) {}
   void stopCustomBlink() {}
   byte getState() { return IOTWEBCONF_STATE_ONLINE; }
   void setApTimeoutMs(unsigned long apTimeoutMs) { }
   unsigned long getApTimeoutMs() { return 0; }
   void resetWifiAuthInfo() {}
   IotWebConfParameter* getApTimeoutParameter() {
      return &apiTimeoutParameter;
   }

private:
   IotWebConfParameter apiTimeoutParameter;

};

#endif // IOTWEBCONFIG_H
