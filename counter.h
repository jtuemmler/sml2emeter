#ifndef COUNTER_H
#define COUNTER_H

#include <stdint.h>

/**
* Class that implements a counter that is persisted in flash-memory.
*/
class Counter {
public:
   /**
   * @brief Create a counter and restore the latest state from flash.
   * @param sector First sector used for persisting the counter.
   * @param sectorSize Size (in bytes) of a sector.
   */
   Counter();

   void init(uint16_t sector, uint32_t sectorSize);
   
   /**
   * @brief Increment the counter by one.
   */
   void increment();

   /**
   * @brief Get the current value of the counter.
   */
   uint32_t get();
   
   /**
   * @brief Print the internal state of the counter.
   */
   void info();

private:
   // Size of a sector in flash-memory
   uint16_t sectorSize;
   
   // Start offsets of the two flash-memory blocks used for counting
   uint32_t blockStart[2];
   
   // Current bits which are used for counting
   uint32_t currentBits;
   
   // Current value of the counter
   uint32_t currentValue;
   
   // Active block which is used for counting
   int activeBlock;
   
   // Offset in the active block
   int blockOffset;
   
   // Indicates whether the counter is successfully initialized
   bool initalized;

   /// Count the bits which are *cleared* in the given DWORD.
   static uint32_t countBits(uint32_t value);

   /// Clear the next available bit. Returns true, if there was no bit to clear left (overflow).
   static bool incrementBits(uint32_t &value);

   /// Initialize the flash
   void initFlash();

   /// Initialize a block
   void initializeBlock(int blockId, uint32_t startValue);

   /// Restore the counter from flash
   void restoreCounter(uint32_t startValue);

   /// Switch the active block
   void switchBlock();
};


#endif // COUNTER_H
