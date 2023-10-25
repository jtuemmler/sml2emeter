#ifndef PULSE_COUNTER_H
#define PULSE_COUNTER_H

#include <stdint.h>

/**
* @brief Initialize pulse counting and restore last state from flash.
* @param inputPin Pin to use for pulse-counting.
* @param debugPin Pin to use for debugging.
* @param sector First sector used for persisting the counter.
* @param sectorSize Size (in bytes) of a sector.
*/
void initPulseCounter(int inputPin, int debugPin, uint16_t sector, uint32_t sectorSize);

/**
* @brief Persist current state to flash.
*/
void storePulseCounter();

/**
* @brief Update internal configuration.
*/
void updatePulseCounterConfig(int pulseTimeoutMs, float pulseFactor);

/**
* @brief Get current data.
* @param[out] impulsesOut  Current number of detected impulses.
* @param[out] m3Out        Current volume in m3.
*/
void getPulseCounter(unsigned long& impulsesOut, float& m3Out);

#endif // PULSE_COUNTER_H
