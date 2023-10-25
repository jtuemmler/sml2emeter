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

   operator const char*() { return std::string::c_str(); }

   String& operator += (const String &other) {
      std::string::append(other);
      return *this;
   }

   String& operator += (const char pOther[]) {
      std::string::append(pOther);
      return *this;
   }

   String& operator += (const double other) {
      char converted[20] = { 0 };
      snprintf(converted, sizeof(converted) - 1, "%g", other);
      std::string::append(converted);
      return *this;
   }

   friend String operator + (const String &LHS, const String &RHS) {
      String s(LHS);
      s.append(RHS);
      return s;
   }

   friend String operator + (const String &LHS, const char RHS[]) {
      String s(LHS);
      s.append(RHS);
      return s;
   }

   friend String operator + (const char LHS[], const String& RHS) {
      String s(LHS);
      s.append(RHS);
      return s;
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
#define CHANGE 3
#define INPUT_PULLUP 4
#define INPUT 0
#define OUTPUT 1
#define D1 1
#define D2 2
#define D3 3
#define D4 4
#define D5 5

void digitalWrite(byte gpio, byte value);
byte digitalRead(byte gpio);
void pinMode(byte gpio, byte value);
byte digitalPinToInterrupt(byte gpio);
void attachInterrupt(byte gpio, void (*interrupHandler)(), byte type);
void detachInterrupt(byte gpio);

// ----------------------------------------------------------------------------
// Interrupts
// ----------------------------------------------------------------------------
void interrupts();
void noInterrupts();
#define ICACHE_RAM_ATTR

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
   String toString() const { return String("aaa.bbb.ccc.ddd"); }
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
      _testDataPos = 0;
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
   int read();
   operator bool() const { return true; }
private:
   const char* _pFileName;
   int _timeout;
   int _fd;
   int _testDataPos;
};

extern SerialImpl Serial;

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

extern ESPImpl ESP;

#endif
