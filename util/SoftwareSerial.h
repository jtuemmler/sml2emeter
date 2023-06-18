// ----------------------------------------------------------------------------
// SoftwareSerial stubs to let the sketch compile and run
// on a PC running windows, linux or macos
// ----------------------------------------------------------------------------

#ifndef SOFTWARE_SERIAL_H
#define SOFTWARE_SERIAL_H

#include "Arduino.h"

#define SWSERIAL_8N1 1

class SoftwareSerial {
public:
   SoftwareSerial() {};
   size_t write(uint8_t byte) { return 0; };
   void begin(uint32_t baud, uint8_t serialConfig, uint8_t rxPin, int8_t txPing, bool invert) {};
};

#endif