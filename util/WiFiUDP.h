// ----------------------------------------------------------------------------
// UDP-stubs to let the sketch compile and run
// on a PC running windows, linux or macos
// ----------------------------------------------------------------------------

#ifndef WIFIUDP_H
#define WIFIUDP_H

#include "arduino_stubs.h"

class WiFiUDP {
public:
   WiFiUDP();
   int beginPacket(IPAddress ipAddress, uint16_t port);
   int beginPacketMulticast(IPAddress multicastAddress,
                            uint16_t port,
                            IPAddress interfaceAddress,
                            int ttl = 1);
   int write(const byte *pBuffer, int length);
   int endPacket();
private:
   static const int MAX_LENGTH = 1500;
   struct sockaddr_in _address;
   int _socket;
   int _pos;
   int _length;
   byte _buffer[MAX_LENGTH];
};

#endif
