// ----------------------------------------------------------------------------
// SPI flash emulation for PCs
// ----------------------------------------------------------------------------

#ifndef SPI_FLASH_H
#define SPI_FLASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

   #define SECTOR_SIZE 512

   typedef enum {
      SPI_FLASH_RESULT_OK,
      SPI_FLASH_RESULT_ERR,
      SPI_FLASH_RESULT_TIMEOUT
   } SpiFlashOpResult;

   SpiFlashOpResult spi_flash_erase_sector(uint16_t sec);
   SpiFlashOpResult spi_flash_write(uint32_t des_addr, uint32_t *src_addr, uint32_t size);
   SpiFlashOpResult spi_flash_read(uint32_t src_addr, uint32_t *des_addr, uint32_t size);

   uint32_t getEraseCounter();

#ifdef __cplusplus
}
#endif

#endif