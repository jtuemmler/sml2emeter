#include <stdio.h>
#include <stdint.h>
#include "spi_flash.h"
#include "counter.h"

uint32_t buffer[1024];

void readBuffer(int sector) {
   SpiFlashOpResult result;
   result = spi_flash_read(sector * 4096, buffer, sizeof(buffer));
   printf("Read result %d\n", result);

   printf("Buffer\n");
   for (int i = 0; i < 10; ++i) {
      printf("%08x  ", buffer[i]);
   }
   printf("\nBufferEnd\n");
}

void write(uint32_t value) {
   SpiFlashOpResult result;
   buffer[0] = value;
   result = spi_flash_write(1000 * 4096, buffer, 4);
   printf("Write result %d\n", result);

   buffer[0] = 0;
   result = spi_flash_read(1000 * 4096, buffer, 4);
   printf("Read result %d\n", result);
   printf("Difference %d\n", buffer[0] - value);
}

void testFlash() {
   SpiFlashOpResult result;

   int sector = 1000;
   uint32_t sectorSize = 4096;

   readBuffer(1000);

   write(0x7FFFFFFF);
   write(0x3FFFFFFF);
   write(0x1FFFFFFF);
   write(0x0FFFFFFF);
   write(0x07FFFFFF);
   write(0x03FFFFFF);
   write(0x01FFFFFF);
   write(0x00FFFFFF);

   result = spi_flash_erase_sector(sector);
   printf("Erase result %d\n", result);

   write(0x7FFFFFFF);
   write(0x3FFFFFFF);
   write(0x1FFFFFFF);
   write(0x0FFFFFFF);
   write(0x07FFFFFF);
   write(0x03FFFFFF);
   write(0x01FFFFFF);
   write(0x00FFFFFF);
}

int main(int argc, char **argv) {
   uint32_t expectedCounter = 0;
   int errors = 0;

   for (int i = 0; i < 10000; ++i) {
      Counter counter;
      counter.init(0, SECTOR_SIZE);
      printf("Counter at start: %d\n", counter.get());
      if (expectedCounter != counter.get()) {
         printf("ERROR: %d: Counter expected %d, is %d\n", i, expectedCounter, counter.get());
         ++errors;
      }
      for (int j = 0; j < i; ++j) {
         ++expectedCounter;
         counter.increment();
      }
      counter.info();
      printf("Counter at end  : %d\n", counter.get());
   }

   printf("\nErase-counter   : %d\n", getEraseCounter());
   printf("Errors          : %d\n", errors);

   return 0;
}