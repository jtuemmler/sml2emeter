#include <Arduino.h>
#include <spi_flash.h>
#include "counter.h"

const uint32_t HEADER_ID = 0x52425300;
const uint32_t HEADER_SIZE = 2 * sizeof(uint32_t);
const uint32_t EMPTY_BIT_PATTERN = 0xffffffff;
const int READ_BUFFER_SIZE = 64;

//#define PRINT(MSG) Serial.print(MSG)
//#define PRINTLN(MSG) Serial.println(MSG)
//#define PRINTNUMLN(NUM) Serial.println(NUM)

//#define PRINT(MSG) printf(MSG)
//#define PRINTLN(MSG) printf("%s\n",MSG)
//#define PRINTNUMLN(NUM) printf("%d\n",NUM)

#define PRINT(MSG) 
#define PRINTLN(MSG) 
#define PRINTNUMLN(NUM) 

uint32_t Counter::countBits(uint32_t value)
{
   uint32_t result = 0;

   // Special case -> return directly, if all bits are cleared.
   if (value == 0UL) {
      return 32;
   }

   // Otherwise count the *cleared* bits.
   for (int i = 0; i < 32; ++i) {
      result += (1 - (value & 1));
      value >>= 1;
   }
   return result;
}

bool Counter::incrementBits(uint32_t & value)
{
   uint32_t bitMask = 1;
   for (int i = 0; i < 32; ++i) {
      if (value & bitMask) {
         value = value & ~bitMask;
         // One bit was cleared, no overflow.
         return false;
      }
      bitMask <<= 1;
   }
   // Overflow
   return true;
}

Counter::Counter() : sectorSize(0), currentBits(0), currentValue(0), activeBlock(0), blockOffset(0), initalized(false)
{ }

void Counter::init(uint16_t sector, uint32_t sectorSize)
{
   this->sectorSize = sectorSize;
   blockStart[0] = sector * sectorSize;
   blockStart[1] = blockStart[0] + sectorSize;
   initFlash();
   info();
}

void Counter::info() {
   PRINT("Active  buffer: "); 
   PRINTNUMLN(activeBlock);
   PRINT("Current offset: "); 
   PRINTNUMLN(blockOffset);
   PRINT("Current value : "); 
   PRINTNUMLN(currentValue);
   PRINT("Current bits  : "); 
   PRINTNUMLN(currentBits);
}

void Counter::initFlash()
{
   uint32_t header[2][2];

   // Check if blocks contain valid headers ...
   for (int i = 0; i < 2; ++i) {
      SpiFlashOpResult result = spi_flash_read(blockStart[i], header[i], HEADER_SIZE);
      if (result != SPI_FLASH_RESULT_OK) {
         PRINTLN("ERROR: initFlash: Couldn't read header!");
         return;
      }
   }

   if ((header[0][0] != HEADER_ID) || (header[1][0] != HEADER_ID)) {
      // No, erase blocks ...
      PRINTLN("Initializing blocks ...");
      // Somehow we have to mark the active buffer. This is done be checking the offset at the beginning.
      // The buffer with the highest offset is active. 
      initializeBlock(0, 0);
      header[0][1] = 0;
      initializeBlock(1, 0);
      header[1][1] = 0;
   }

   activeBlock = header[0][1] > header[1][1] ? 0 : 1;
   restoreCounter(header[activeBlock][1]);
}

void Counter::initializeBlock(int blockId, uint32_t startValue) {
   SpiFlashOpResult result = spi_flash_erase_sector(blockStart[blockId] / sectorSize);
   if (result != SPI_FLASH_RESULT_OK) {
      PRINTLN("ERROR: initializeBlock: Erase failed!");
      return;
   }

   uint32_t header[2];
   header[0] = HEADER_ID;
   header[1] = startValue;

   result = spi_flash_write(blockStart[blockId], header, HEADER_SIZE);
   if (result != SPI_FLASH_RESULT_OK) {
      PRINTLN("ERROR: initializeBlock: Write failed!");
      return;
   }
}

void Counter::restoreCounter(uint32_t startValue)
{
   currentValue = startValue;
   blockOffset = HEADER_SIZE;
   uint32_t counterBits[READ_BUFFER_SIZE];
   uint32_t *pCurrentBits;

   while (blockOffset < sectorSize) {
      SpiFlashOpResult result = spi_flash_read(blockStart[activeBlock] + blockOffset, counterBits, sizeof(counterBits));

      if (result != SPI_FLASH_RESULT_OK) {
         PRINTLN("ERROR: restoreCounter: Could not read flash!");
         return;
      }

      pCurrentBits = counterBits;
      for (int i = 0; i < READ_BUFFER_SIZE; ++i) {
         if (blockOffset >= sectorSize) {
            break;
         }
         if (*pCurrentBits == EMPTY_BIT_PATTERN) {
            if (pCurrentBits != counterBits) {
               blockOffset -= sizeof(uint32_t);
               --pCurrentBits;
            }
            goto restoreFinished;
         }
         currentValue += countBits(*(pCurrentBits++));
         blockOffset += sizeof(uint32_t);
      }
   }

restoreFinished:
   currentBits = *pCurrentBits;
   initalized = true;
}

void Counter::switchBlock()
{
   activeBlock = (activeBlock + 1) % 2;
   initializeBlock(activeBlock, currentValue);
   blockOffset = HEADER_SIZE;
}

void Counter::increment()
{
   if (initalized) {
      bool overflow = incrementBits(currentBits);
      if (overflow) {
         blockOffset += sizeof(uint32_t);
         if (blockOffset >= sectorSize) {
            switchBlock();
         }
         currentBits = EMPTY_BIT_PATTERN;
         incrementBits(currentBits);
      }

      if (spi_flash_write(blockStart[activeBlock] + blockOffset, &currentBits, sizeof(uint32_t)) == SPI_FLASH_RESULT_OK) {
         //PRINTLN("Counter flashed.");
         ++currentValue;
      }
      else {
         PRINTLN("ERROR: increment: Could not write!");
      }
   }
}

uint32_t Counter::get()
{
   if (initalized) {
      return currentValue;
   }
   return 0x0;
}
