#include <Arduino.h>
#include "pulsecounter.h"
#include "counter.h"

// Input pin for detecting impulses
static int inputPin;

// Indicates, whether pulse-counting is enabled
static volatile bool enabled;

// Timeout for pulse-counting. If set to 0, impulse counting is turned off.
static volatile unsigned long pulseTimeoutMs;

// Indicates wether the beginning of an impulse has been detected
static volatile bool isrArmed;

// Indicates the last state of the impulse-pin (HIGH / LOW)
static volatile int isrLastState;

// Counted impulses
static volatile unsigned long impulses;

// Counted interrupts
static volatile unsigned long interruptCount;

// Time (ticks) of the last received impulse
static volatile unsigned long lastPulseEventMs;

// Factor for m3 calculation
static float pulseFactor;

// Persisted instance of the impulse counter
static Counter impulseCounter;

ICACHE_RAM_ATTR static void handleInterrupt() {
   if (enabled) {
      // Check whether the state if the dection pin has been changed.
      int state = digitalRead(inputPin);
      if (state == isrLastState) {
         return;
      }
      isrLastState = state;

      // State has changed ...
      unsigned long currentTimeMs = millis();

      // If we changed from HIGH -> LOW, the beginning of an impulse has been detected.
      // Now wait until the signal is released ...
      if (state == LOW) {
         lastPulseEventMs = currentTimeMs;
         isrArmed = true;
      }
      else if ((state == HIGH) && isrArmed) {
         // Signal was released and we've detected the beginning before.
         isrArmed = false;
         // Now check if the debounce-timeout has been elapsed. If so, count the impulse.
         if (((currentTimeMs - lastPulseEventMs) > pulseTimeoutMs) || (currentTimeMs < lastPulseEventMs)) {
            ++impulses;
            //Serial.print("i");
         }
      }
   }
}

void initPulseCounter(int inputPinIn, uint16_t sector, uint32_t sectorSize)
{
   inputPin = inputPinIn;
   pulseTimeoutMs = 0UL;
   isrArmed = false;
   isrLastState = -2;
   impulses = 0UL;
   interruptCount = 0UL;
   lastPulseEventMs = 0UL;
   pulseFactor = 0.01f;

   pinMode(inputPin, INPUT_PULLUP);

   impulseCounter.init(sector, sectorSize);
   impulses = impulseCounter.get();
}

void storePulseCounter() {
   if (enabled) {
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

void updatePulseCounterConfig(int pulseTimeoutMsIn, float pulseFactorIn) {
   pulseTimeoutMs = (unsigned long)pulseTimeoutMsIn;
   pulseFactor = pulseFactorIn;

   enabled = pulseTimeoutMs > 0;

   if (enabled) {
      // Attach interrupt-handler
      Serial.print("Pulse-Pin: ");
      Serial.println(inputPin);
      Serial.print("Interrupt: ");
      Serial.println(digitalPinToInterrupt(inputPin));
      attachInterrupt(digitalPinToInterrupt(inputPin), handleInterrupt, CHANGE);
   }
   else {
      detachInterrupt(digitalPinToInterrupt(inputPin));
   }
}

void getPulseCounter(unsigned long& impulsesOut, float& m3Out) {
   if (enabled) {
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
