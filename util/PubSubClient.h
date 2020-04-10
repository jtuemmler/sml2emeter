// ----------------------------------------------------------------------------
// PubSubClient-stubs to let the sketch compile and run
// on a PC running windows, linux or macos
// ----------------------------------------------------------------------------

#ifndef PUBSUBCLIENT_H
#define PUBSUBCLIENT_H

class WiFiClient {
public:
   WiFiClient() {}
};

class PubSubClient {
public:
   PubSubClient(WiFiClient &wifiClient) {}

   void setServer(String hostName, int port) {}
   bool connect(String clientId) { return true; }
   bool connected() { return true; }
   void disconnect() {}
   int state() { return 0; }
   int publish(const char *pTopic, const char* pData) { return 0; }
   void loop() {}
};

#endif
