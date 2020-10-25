#include <stdint.h>
#include <stdio.h>
#include "spi_flash.h"

const uint32_t FLASH_SIZE_IN_U32 = 1048576;
const uint32_t SECTOR_SIZE_IN_U32 = SECTOR_SIZE / sizeof(uint32_t);
const uint32_t EMPTY_PATTERN = 0xffffffff;

uint32_t flashMem[FLASH_SIZE_IN_U32];
uint32_t eraseCounter = 0;

SpiFlashOpResult spi_flash_erase_sector(uint16_t sec) {
   ++eraseCounter;
   int offset = sec * SECTOR_SIZE_IN_U32;
   for (uint32_t i = 0; i < SECTOR_SIZE_IN_U32; ++i) {
      flashMem[i + offset] = EMPTY_PATTERN;
   }

   return SPI_FLASH_RESULT_OK;
}

SpiFlashOpResult spi_flash_write(uint32_t des_addr, uint32_t *src_addr, uint32_t size) {
   size /= sizeof(uint32_t);
   des_addr /= sizeof(uint32_t);
   for (uint32_t i = 0; i < size; ++i) {
      flashMem[des_addr + i] = ((flashMem[des_addr + i] ^ EMPTY_PATTERN) | (src_addr[i] ^ EMPTY_PATTERN)) ^ EMPTY_PATTERN;
   }

   // Verify
   for (uint32_t i = 0; i < size; ++i) {
      if (flashMem[des_addr + i] != src_addr[i]) {
         printf("Inconsistent data!\n");
      }
   }


   return SPI_FLASH_RESULT_OK;
}

SpiFlashOpResult spi_flash_read(uint32_t src_addr, uint32_t *des_addr, uint32_t size) {
   size /= sizeof(uint32_t);
   src_addr /= sizeof(uint32_t);
   for (uint32_t i = 0; i < size; ++i) {
      des_addr[i] = flashMem[src_addr + i];
   }

   return SPI_FLASH_RESULT_OK;
}

uint32_t getEraseCounter() {
   return eraseCounter;
}