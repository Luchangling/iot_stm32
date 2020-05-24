
#ifndef __I2C_DRIVER_H__
#define __I2C_DRIVER_H__

#include "stm32f10x.h"

void Sensor_I2C_Init(void);

void Sensor_I2C_ByteWrite(u8 slaveAddr, u8* pBuffer, u8 writeAddr);

void Sensor_I2C_BufferRead(u8 slaveAddr, u8* pBuffer, u8 readAddr, u16 NumByteToRead);

void Sensor_WriteBits(uint8_t slaveAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data);

void Sensor_WriteBit(uint8_t slaveAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data);

void Sensor_ReadBits(uint8_t slaveAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t *data);

void Sensor_ReadBit(uint8_t slaveAddr, uint8_t regAddr, uint8_t bitNum, uint8_t *data);

#endif

