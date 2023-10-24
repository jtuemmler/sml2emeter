#include <Arduino.h>
#include "pulsecounter.h"


ICACHE_RAM_ATTR PulseCounter& PulseCounter::getInstance()
{
   static PulseCounter pulseCounter;
   return pulseCounter;
}

ICACHE_RAM_ATTR void PulseCounter::handleInterrupt() {
   getInstance().handleInterruptInternal();
}

ICACHE_RAM_ATTR void PulseCounter::handleInterruptInternal() {
   // Check whether impulses should be detected.
   if (pulseTimeoutMs > 0) {
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
         isrArmed = 1;
      }
      else if ((state == HIGH) && (isrArmed == 1)) {
         // Signal was released and we've detected the beginning before.
         isrArmed = 0;
         // Now check if the debounce-timeout has been elapsed. If so, count the impulse.
         if (((currentTimeMs - lastPulseEventMs) > pulseTimeoutMs) || (currentTimeMs < lastPulseEventMs)) {
            ++impulses;
            //Serial.print("i");
         }
      }
   }
}

PulseCounter::PulseCounter() : inputPin(0), pulseTimeoutMs(0UL), isrArmed(0), isrLastState(-2), impulses(0UL), interruptCount(0UL), lastPulseEventMs(0UL), pulseFactor(0.01f) {
}

void PulseCounter::init(int inputPin, uint16_t sector, uint32_t sectorSize)
{
   this->inputPin = inputPin;
   pinMode(inputPin, INPUT_PULLUP);

   impulseCounter.init(sector, sectorSize);
   impulses = impulseCounter.get();
}

void PulseCounter::store() {
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


void PulseCounter::updateConfig(int pulseTimeoutMs, float pulseFactor) {
   this->pulseTimeoutMs = (unsigned long)pulseTimeoutMs;
   this->pulseFactor = pulseFactor;

   if (pulseTimeoutMs > 0) {
      // Attach interrupt-handler
      Serial.print("Pulse-Pin: ");
      Serial.println(inputPin);
      Serial.print("Interrupt: ");
      Serial.println(digitalPinToInterrupt(inputPin));
      attachInterrupt(digitalPinToInterrupt(inputPin), PulseCounter::handleInterrupt, CHANGE);
   }
   else {
      detachInterrupt(digitalPinToInterrupt(inputPin));
   }
}

void PulseCounter::get(unsigned long& impulsesOut, float& m3Out) {
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
