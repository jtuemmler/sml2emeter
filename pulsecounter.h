#ifndef PULSE_COUNTER_H
#define PULSE_COUNTER_H

#include <stdint.h>
#include "counter.h"

/**
* Class to implement pulse-counting to support gas-meters..
*/
class PulseCounter {
   private:
      // Input pin for detecting impulses
      int inputPin;

      // Timeout for pulse-counting. If set to 0, impulse counting is turned off.
      volatile unsigned long pulseTimeoutMs;

      // Indicates wether the beginning of an impulse has been detected
      volatile int isrArmed;

      // Indicates the last state of the impulse-pin (HIGH / LOW)
      volatile int isrLastState;

      // Counted impulses
      volatile unsigned long impulses;

      // Counted interrupts
      volatile unsigned long interruptCount;

      // Time (ticks) of the last received impulse
      volatile unsigned long lastPulseEventMs;

      // Factor for m3 calculation
      float pulseFactor;

      // Persisted instance of the impulse counter
      Counter impulseCounter;

      /**
      * @brief Constructor.
      */
      PulseCounter();
      
      /**
      * @brief Constructor.
      */
      static void handleInterrupt();

      /**
      * @brief Handle the interrupt of the input-pin.
      */
      void handleInterruptInternal();

   public:
 
      /**
      * @brief Return the instance of the PulseCounter.
      */
      static PulseCounter& getInstance();

      /**
      * @brief Initialize pulse counting and restore last state from flash.
      * @param inputPin Pin to use for pulse-counting.
      * @param sector First sector used for persisting the counter.
      * @param sectorSize Size (in bytes) of a sector.
      */
      void init(int inputPin, uint16_t sector, uint32_t sectorSize);

      /**
      * @brief Persist current state to flash.
      */
      void store();

      /**
      * @brief Update internal configuration.
      */
      void updateConfig(int pulseTimeoutMs, float pulseFactor);

      /**
      * @brief Get current data.
      * @param[out] impulsesOut  Current number of detected impulses.
      * @param[out] m3Out        Current volume in m3.
      */
      void get(unsigned long& impulsesOut, float& m3Out);

};


#endif // PULSE_COUNTER_H
