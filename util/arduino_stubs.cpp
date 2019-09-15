// ----------------------------------------------------------------------------
// Some general purpose stubs to let the sketch compile and run
// on a PC running windows, linux or macos
// ----------------------------------------------------------------------------

#include "arduino_stubs.h"
#include "sml_testpacket.h"

#ifndef _WIN32
#  include <time.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <sys/types.h>
#  include <termios.h>
#endif

#ifdef __MACH__
#  include <mach/clock.h>
#  include <mach/mach.h>
#endif

void delay(unsigned long duration) {
#if _WIN32
   Sleep(duration);
#else
   usleep(1000UL * duration);
#endif
}

unsigned long millis() {
#if _WIN32
   return GetTickCount();
#elif __MACH__
   clock_serv_t cclock;
   mach_timespec_t mts;
   host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
   clock_get_time(cclock, &mts);
   mach_port_deallocate(mach_task_self(), cclock);
   return (mts.tv_sec * 1000UL) + (mts.tv_nsec / 1000000UL);
#else
   struct timespec tv;
   clock_gettime(CLOCK_MONOTONIC, &tv);
   return (tv.tv_sec * 1000UL) + (tv.tv_nsec / 1000000UL);
#endif
}

void digitalWrite(byte gpio, byte value) {}

void pinMode(byte gpio, byte value) {}

SerialImpl::~SerialImpl()
{
#ifndef _WIN32
   if (_fd >= 0)
   {
      close(_fd);
   }
#endif
}

bool SerialImpl::available() {
   int bytesAvailable = 0;
   if (_fd < 0) {
      delay(1);
      bytesAvailable = 1;
      return (bytesAvailable > 0);
   }
#ifndef _WIN32
   ioctl(_fd,FIONREAD,&bytesAvailable);
#endif
   return (bytesAvailable > 0);
}

void SerialImpl::begin(int baud) {
#ifndef _WIN32
   if (_pFileName != NULL) {
      _fd = open(_pFileName, O_RDWR | O_NOCTTY | O_NDELAY);
      // Set blocking mode
      fcntl(_fd, F_SETFL, 0);
   }
#endif
}

int SerialImpl::readBytes(byte *pBuffer, int bufferSize) {
   int length = 0;
   if (_fd < 0) {
      delay(_timeout);
      length = bufferSize < SML_TEST_PACKET_LENGTH ? bufferSize : SML_TEST_PACKET_LENGTH;
      memcpy(pBuffer, SML_TEST_PACKET, length);
      return length;
   }
#ifndef _WIN32
   int timeout = _timeout;
   while ((timeout > 0) && (bufferSize > 0)) {
      if (available()) {
         int bytesRead = read(_fd,(void*)(pBuffer + length),bufferSize);
         bufferSize -= bytesRead;
         length += bytesRead;
         timeout = _timeout;
      }
      else {
         delay(100);
         timeout -= 100;
      }
   }
#endif
   return length;
}

#ifndef _WIN32
char *itoa(int value, char *str, int base)
{
   sprintf(str,"%d",value);
   return str;
}
#endif

String::String() : std::string() {}

String::String(const char pOther[]) : std::string(pOther) {}
