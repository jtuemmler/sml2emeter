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
   PubSubClient(WiFiClient &wifiClient) : _connected(false) {}

   void setServer(String hostName, int port) {}
   bool connect(String clientId) { _connected = true;  return true; }
   bool connected() { return _connected; }
   void disconnect() { _connected = false; }
   int state() { return _connected ? 0 : -1; }
   int publish(const char *pTopic, const char* pData) {
      printf(" Publish %s : %s\n", pTopic, pData);
      return _connected ? 0 : -1;
   }
   void loop() {}

private:
   bool _connected;
};

#endif
