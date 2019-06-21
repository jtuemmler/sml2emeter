// ----------------------------------------------------------------------------
// Main function to emulate the arduino behaviour in a regular c/c++ environment
// ----------------------------------------------------------------------------

// Change serial number if running on a PC
#define EMETER_SERNO 994420617

#include "arduino_stubs.h"

SerialImpl Serial;
ESPImpl ESP;

#include "../sml2emeter.ino"

int main(int argc, char** argv) {
   if (argc > 1) {
      printf("Using %s for reading SML data.\n", argv[1]);
      Serial.setFile(argv[1]);
   }

   setup();

   while (true) {
      loop();
   }

   return 0;
}
