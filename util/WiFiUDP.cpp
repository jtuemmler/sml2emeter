// ----------------------------------------------------------------------------
// UDP-stubs to let the sketch compile and run
// on a PC running windows, linux or macos
// ----------------------------------------------------------------------------

#include "WiFiUDP.h"
#include "arduino_stubs.h"

#ifndef WSAGetLastError
#  include <errno.h>
#  define WSAGetLastError() (errno)
#endif

#ifndef SOCKET_ERROR
#  define SOCKET_ERROR (-1)
#endif

WiFiUDP::WiFiUDP() : _pos(0), _length(0) {
#ifdef _WIN32
   WSADATA wsa;

   if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
   {
      printf("WSAStartup() failed with error code : %d" , WSAGetLastError());
      exit(EXIT_FAILURE);
   }
#endif

   if ((_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
   {
      printf("socket() failed with error code : %d", WSAGetLastError());
      exit(EXIT_FAILURE);
   }
}

int WiFiUDP::beginPacket(IPAddress ipAddress, uint16_t port) {
   _address = ipAddress.getAddress();
   _address.sin_port = htons(port);
   _pos = 0;
   _length = 0;
   return 1;
}

int WiFiUDP::beginPacketMulticast(IPAddress multicastAddress, uint16_t port, IPAddress interfaceAddress, int ttl) {
   return beginPacket(multicastAddress, port);
}

int WiFiUDP::write(const byte *pBuffer, int length) {
   if (_pos + length < MAX_LENGTH) {
      memcpy(_buffer + _pos, pBuffer, length);
      _pos += length;
      this->_length += length;
      return 1;
   }
   return 0;
}

int WiFiUDP::endPacket() {
   if (sendto(_socket, (char*)_buffer, _length, 0, (struct sockaddr *) &_address, sizeof(_address)) == SOCKET_ERROR) {
      printf("sendto() failed with error code : %d", WSAGetLastError());
   }
   return 1;
}
