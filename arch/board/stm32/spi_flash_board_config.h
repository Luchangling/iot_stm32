
#ifndef __SPI_FLASH_BOARD_CONFIG_H__
#define __SPI_FLASH_BOARD_CONFIG_H__
#include "stm32f10x_spi.h"
#include "bit_band_define.h"
#include "gpio_board_config.h"

void spi_flash_init(void);

u16 SPI_Flash_ReadID(void);

void SPI_Flash_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead);

void SPI_Flash_Write(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite);

void SPI_Flash_PowerDown(void);

void SPI_Flash_WAKEUP(void);

void SPI_Flash_Write_NoCheck(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite);


#endif

