// ----------------------------------------------------------------------------
// Some general purpose stubs to let the sketch compile and run
// on a PC running windows, linux or macos
// ----------------------------------------------------------------------------

#ifndef ARDUINO_STUBS_H
#define ARDUINO_STUBS_H

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <string>
#include <stdlib.h>

using namespace std;

#if _WIN32
#  define _WINSOCK_DEPRECATED_NO_WARNINGS
#  include <winsock2.h>
#else
#  include <netinet/in.h>
#  include <arpa/inet.h>
#endif

typedef uint8_t byte;
typedef uint16_t word;

// ----------------------------------------------------------------------------
// String handling
// ----------------------------------------------------------------------------

class String : public std::string {
public:
   String();
   String(const char pOther[]);

   void replace(const char s1[], const char s2[]) {}

   operator const char*()  { return std::string::c_str(); }

   String& operator += (const char pOther[]) {
      std::string::append(pOther);
      return *this;
   }

   String& operator += (const double other) {
      char converted[20] = { 0 };
      snprintf(converted,sizeof(converted) - 1, "%g", other);
      std::string::append(converted);
      return *this;
   }
};

#ifndef _WIN32
char *itoa(int value, char * str, int base);
#endif

// ----------------------------------------------------------------------------
// Time
// ----------------------------------------------------------------------------
void delay(unsigned long duration);
unsigned long millis();

// ----------------------------------------------------------------------------
// GPIOs
// ----------------------------------------------------------------------------
#define LED_BUILTIN 1
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1

void digitalWrite(byte gpio, byte value);
void pinMode(byte gpio, byte value);

// ----------------------------------------------------------------------------
// IPAddress
// ----------------------------------------------------------------------------
class IPAddress {
public:
   IPAddress() {
      memset((char *)&_address, 0, sizeof(_address));
   }

   IPAddress(byte a, byte b, byte c, byte d) {
      memset((char *)&_address, 0, sizeof(_address));
      _address.sin_family = AF_INET;
#ifdef _WIN32
      _address.sin_addr.S_un.S_un_b.s_b1 = a;
      _address.sin_addr.S_un.S_un_b.s_b2 = b;
      _address.sin_addr.S_un.S_un_b.s_b3 = c;
      _address.sin_addr.S_un.S_un_b.s_b4 = d;
#else
      _address.sin_addr.s_addr = htonl((int)a << 24 | (int)b << 16 | (int)c << 8 | d);
#endif
   }
   operator const char*() const { return inet_ntoa(_address.sin_addr); }
   struct sockaddr_in getAddress() { return _address; }
   bool fromString(const String &address) { return false; }
private:
   struct sockaddr_in _address;
};

// ----------------------------------------------------------------------------
// Serial implementation
// ----------------------------------------------------------------------------
class SerialImpl {
public:
   SerialImpl() {
      _pFileName = NULL;
      _timeout = 1000;
      _fd = -1;
   }
   ~SerialImpl();
   void print(const char* msg) {
      printf("%s", msg);
   }
   void print(int num) {
      printf("%d", num);
   }
   void println(const char* msg = "") {
      printf("%s\n", msg);
   }
   void println(int num) {
      printf("%d\n", num);
   }
   void setTimeout(int value) {
      _timeout = value;
   }
   void setFile(const char *pFileName) {
      _pFileName = pFileName;
   }
   bool available();
   void begin(int baud);
   int readBytes(byte *pBuffer, int bufferSize);
   operator bool() const { return true; }
private:
   const char* _pFileName;
   int _timeout;
   int _fd;
};

// ----------------------------------------------------------------------------
// ESP implementation
// ----------------------------------------------------------------------------
class ESPImpl {
public:
   ESPImpl() {}
   int getChipId() {
      return 0;
   }
   int getFlashChipId() {
      return 0;
   }
   void restart() {
      printf("\nRestart!");
      exit(-1);
   }
};

#endif
