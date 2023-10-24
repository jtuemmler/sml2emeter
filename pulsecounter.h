#ifndef PULSE_COUNTER_H
#define PULSE_COUNTER_H

#include <stdint.h>
#include "counter.h"

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

      // Persisted instance of the impulse counter
      Counter impulseCounter;

      // Factor for m3 calculation
      float pulseFactor;

   public:
      /**
         @brief Store the detected number of impulses in flash.
      */
      PulseCounter(int inputPin) : pulseTimeoutMs(0UL), isrArmed(0), isrLastState(-2), impulses(0UL), interruptCount(0UL), lastPulseEventMs(0UL), pulseFactor(0.01f) {
         this->inputPin = inputPin;
         pinMode(inputPin, INPUT_PULLUP);
      }

      void init(uint16_t sector, uint32_t sectorSize)
      {
         impulseCounter.init(sector, sectorSize);
         impulses = impulseCounter.get();
      }

      void store() {
         if (pulseTimeoutMs > 0) {
            uint32_t currentImpulses = 0;
            noInterrupts();
            currentImpulses = impulses;
            interrupts();
            while (currentImpulses > impulseCounter.get()) {
               Serial.print("s");
               impulseCounter.increment();
            }
         }
      }

      bool handleInterrupt() {
         bool updated = false;
         
         // Check whether impulses should be detected.
         if (pulseTimeoutMs > 0) {
            // Check whether the state if the dection pin has been changed.
            int state = digitalRead(inputPin);
            if (state == isrLastState) {
               return updated;
            }
            isrLastState = state;

            // State has changed ...
            unsigned long currentTimeMs = millis();

            // If we changed from HIGH -> LOW, the beginning of an impulse has been detected.
            // Now wait until the signal is released ...
            if (state == LOW) {
               lastPulseEventMs = currentTimeMs;
               isrArmed = 1;
            }
            else if ((state == HIGH) && (isrArmed == 1)) {
               // Signal was released and we've detected the beginning before.
               isrArmed = 0;
               // Now check if the debounce-timeout has been elapsed. If so, count the impulse.
               if (((currentTimeMs - lastPulseEventMs) > pulseTimeoutMs) || (currentTimeMs < lastPulseEventMs)) {
                  updated = true;
                  ++impulses;
                  Serial.print("i");
               }
            }
         }

         return updated;
      }

      void updateConfig(void(*interrupHandler)(), int pulseTimeoutMs, float pulseFactor) {
         this->pulseTimeoutMs = (unsigned long)pulseTimeoutMs;
         this->pulseFactor = pulseFactor;

         if (pulseTimeoutMs > 0) {
            // Attach interrupt-handler
            Serial.print("Pulse-Pin: ");
            Serial.println(inputPin);
            Serial.print("Interrupt: ");
            Serial.println(digitalPinToInterrupt(inputPin));
            attachInterrupt(digitalPinToInterrupt(inputPin), interrupHandler, CHANGE);
         }
         else {
            detachInterrupt(digitalPinToInterrupt(inputPin));
         }   
      }

      void get(unsigned long &impulsesOut, float &m3Out) {
         if (pulseTimeoutMs > 0) {
            // Fetch data into local storage
            noInterrupts();
            impulsesOut = impulses;
            //unsigned long mqttInterruptCount = interruptCount;
            //unsigned long mqttLastPulseEventMs = lastPulseEventMs;
            interrupts();

         }
         else {
            impulsesOut = 0;
         }
         m3Out = impulsesOut * pulseFactor;
      }
};


#endif // PULSE_COUNTER_H
