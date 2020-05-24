
#include "kk_type.h"
#include "gpio_board_config.h"
#include "stm32f10x_i2c.h"

#define FATAL_BEGIN i=0
#define FATAL_CALC  i++
#define FATAL_END  if(i > 500)return

/**
 * @brief  Initializes the I2C peripheral used to drive the MPU6050
 * @param  None
 * @return None
 */
void Sensor_I2C_Init()
{
    I2C_InitTypeDef I2C_InitStructure;
    //GPIO_InitTypeDef GPIO_InitStructure;

    /* Enable I2C and GPIO clocks */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    #if 0
    /* Configure I2C pins: SCL and SDA */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    #endif
    gpio_type_set(PB10, GPIO_Mode_AF_OD);
    gpio_type_set(PB11, GPIO_Mode_AF_OD);
    /* I2C configuration */
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x68<<1;//MPU6050_DEFAULT_ADDRESS; // MPU6050 7-bit adress = 0x68, 8-bit adress = 0xD0;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 100000;

    /* Apply I2C configuration after enabling it */
    I2C_Init(I2C2, &I2C_InitStructure);
    /* I2C Peripheral Enable */
    I2C_Cmd(I2C2, ENABLE);
}

/**
 * @brief  Writes one byte to the  MPU6050.
 * @param  slaveAddr : slave address MPU6050_DEFAULT_ADDRESS
 * @param  pBuffer : pointer to the buffer  containing the data to be written to the MPU6050.
 * @param  writeAddr : address of the register in which the data will be written
 * @return None
 */
void Sensor_I2C_ByteWrite(u8 slaveAddr, u8* pBuffer, u8 writeAddr)
{
    u16 i = 0;
    // ENTR_CRT_SECTION();

    /* Send START condition */
    I2C_GenerateSTART(I2C2, ENABLE);

    FATAL_BEGIN;
    /* Test on EV5 and clear it */
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT))
    {
        FATAL_CALC;
        FATAL_END;
    }

    /* Send MPU6050 address for write */
    I2C_Send7bitAddress(I2C2, slaveAddr, I2C_Direction_Transmitter);

    FATAL_BEGIN;
    /* Test on EV6 and clear it */
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        FATAL_CALC;
        FATAL_END;
    }

    /* Send the MPU6050's internal address to write to */
    I2C_SendData(I2C2, writeAddr);

    FATAL_BEGIN;
    /* Test on EV8 and clear it */
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        FATAL_CALC;
        FATAL_END;
    }

    /* Send the byte to be written */
    I2C_SendData(I2C2, *pBuffer);

    FATAL_BEGIN;
    /* Test on EV8 and clear it */
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        FATAL_CALC;
        FATAL_END;
    }

    /* Send STOP condition */
    I2C_GenerateSTOP(I2C2, ENABLE);

    // EXT_CRT_SECTION();
}

/**
 * @brief  Reads a block of data from the MPU6050.
 * @param  slaveAddr  : slave address MPU6050_DEFAULT_ADDRESS
 * @param  pBuffer : pointer to the buffer that receives the data read from the MPU6050.
 * @param  readAddr : MPU6050's internal address to read from.
 * @param  NumByteToRead : number of bytes to read from the MPU6050 ( NumByteToRead >1  only for the Mgnetometer readinf).
 * @return None
 */
void Sensor_I2C_BufferRead(u8 slaveAddr, u8* pBuffer, u8 readAddr, u16 NumByteToRead)
{
    u16 i = 0;
    // ENTR_CRT_SECTION();
    FATAL_BEGIN;
    /* While the bus is busy */
    while (I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY))
    {
        FATAL_CALC;
        FATAL_END;
    }

    /* Send START condition */
    I2C_GenerateSTART(I2C2, ENABLE);
    
    FATAL_BEGIN;
    /* Test on EV5 and clear it */
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT))
    {
        FATAL_CALC;
        FATAL_END;
    }

    /* Send MPU6050 address for write */
    I2C_Send7bitAddress(I2C2, slaveAddr, I2C_Direction_Transmitter);

    /* Test on EV6 and clear it */
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        FATAL_CALC;
        FATAL_END;
    }

    /* Clear EV6 by setting again the PE bit */
    I2C_Cmd(I2C2, ENABLE);

    /* Send the MPU6050's internal address to write to */
    I2C_SendData(I2C2, readAddr);

    FATAL_BEGIN;
    /* Test on EV8 and clear it */
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        FATAL_CALC;
        FATAL_END;
    }

    /* Send STRAT condition a second time */
    I2C_GenerateSTART(I2C2, ENABLE);

    FATAL_BEGIN;
    /* Test on EV5 and clear it */
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT))
    {
        FATAL_CALC;
        FATAL_END;
    }

    /* Send MPU6050 address for read */
    I2C_Send7bitAddress(I2C2, slaveAddr, I2C_Direction_Receiver);

    FATAL_BEGIN;
    /* Test on EV6 and clear it */
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
    {
        FATAL_CALC;
        FATAL_END;
    }

    /* While there is data to be read */
    while (NumByteToRead)
    {
        if (NumByteToRead == 1)
        {
            /* Disable Acknowledgement */
            I2C_AcknowledgeConfig(I2C2, DISABLE);

            /* Send STOP Condition */
            I2C_GenerateSTOP(I2C2, ENABLE);
        }

        /* Test on EV7 and clear it */
        if (I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_RECEIVED))
        {
            /* Read a byte from the MPU6050 */
            *pBuffer = I2C_ReceiveData(I2C2);

            /* Point to the next location where the byte read will be saved */
            pBuffer++;

            /* Decrement the read bytes counter */
            NumByteToRead--;
        }
    }

    /* Enable Acknowledgement to be ready for another reception */
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    // EXT_CRT_SECTION();
}


/** Write multiple bits in an 8-bit device register.
 * @param slaveAddr I2C slave device address
 * @param regAddr Register regAddr to write to
 * @param bitStart First bit position to write (0-7)
 * @param length Number of bits to write (not more than 8)
 * @param data Right-aligned value to write
 */
void Sensor_WriteBits(uint8_t slaveAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data)
{
    //      010 value to write
    // 76543210 bit numbers
    //    xxx   args: bitStart=4, length=3
    // 00011100 mask byte
    // 10101111 original value (sample)
    // 10100011 original & ~mask
    // 10101011 masked | value
    uint8_t tmp;
    uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
    Sensor_I2C_BufferRead(slaveAddr, &tmp, regAddr, 1);  
    data <<= (bitStart - length + 1); // shift data into correct position
    data &= mask; // zero all non-important bits in data
    tmp &= ~(mask); // zero all important bits in existing byte
    tmp |= data; // combine data with existing byte
    Sensor_I2C_ByteWrite(slaveAddr, &tmp, regAddr);
}

/** write a single bit in an 8-bit device register.
 * @param slaveAddr I2C slave device address
 * @param regAddr Register regAddr to write to
 * @param bitNum Bit position to write (0-7)
 * @param value New bit value to write
 */
void Sensor_WriteBit(uint8_t slaveAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data)
{
    uint8_t tmp;
    Sensor_I2C_BufferRead(slaveAddr, &tmp, regAddr, 1);
    tmp = (data != 0) ? (tmp | (1 << bitNum)) : (tmp & ~(1 << bitNum));
    Sensor_I2C_ByteWrite(slaveAddr, &tmp, regAddr);
}

/** Read multiple bits from an 8-bit device register.
 * @param slaveAddr I2C slave device address
 * @param regAddr Register regAddr to read from
 * @param bitStart First bit position to read (0-7)
 * @param length Number of bits to read (not more than 8)
 * @param data Container for right-aligned value (i.e. '101' read from any bitStart position will equal 0x05)
 * @param timeout Optional read timeout in milliseconds (0 to disable, leave off to use default class value in readTimeout)
 */
void Sensor_ReadBits(uint8_t slaveAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t *data)
{
    // 01101001 read byte
    // 76543210 bit numbers
    //    xxx   args: bitStart=4, length=3
    //    010   masked
    //   -> 010 shifted
    uint8_t tmp;
    uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
    Sensor_I2C_BufferRead(slaveAddr, &tmp, regAddr, 1);
    tmp &= mask;
    tmp >>= (bitStart - length + 1);
    *data = tmp;
}

/** Read a single bit from an 8-bit device register.
 * @param slaveAddr I2C slave device address
 * @param regAddr Register regAddr to read from
 * @param bitNum Bit position to read (0-7)
 * @param data Container for single bit value
 * @param timeout Optional read timeout in milliseconds (0 to disable, leave off to use default class value in readTimeout)
 */
void Sensor_ReadBit(uint8_t slaveAddr, uint8_t regAddr, uint8_t bitNum, uint8_t *data)
{
    uint8_t tmp;
    Sensor_I2C_BufferRead(slaveAddr, &tmp, regAddr, 1);
    *data = tmp & (1 << bitNum);
}



