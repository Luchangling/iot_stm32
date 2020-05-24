//MPU6050 I2C library for ARM STM32F103xx Microcontrollers - Main source file
//Has bit, byte and buffer I2C R/W functions
// 23/05/2012 by Harinadha Reddy Chintalapalli <harinath.ec@gmail.com>
// Changelog:
//     2012-05-23 - initial release. Thanks to Jeff Rowberg <jeff@rowberg.net> for his AVR/Arduino
//                  based MPU6050 development which inspired me & taken as reference to develop this.
/* ============================================================================================
 MPU6050 device I2C library code for ARM STM32F103xx is placed under the MIT license
 Copyright (c) 2012 Harinadha Reddy Chintalapalli

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ================================================================================================
 */

/* Includes */
#include "MPU6050.h"
#include "stm32f10x_i2c.h"
#include "sensor_service.h"
#include "i2c_driver.h"


/** @defgroup MPU6050_Library
 * @{
 */

/** Power on and prepare for general usage.
 * This will activate the device and take it out of sleep mode (which must be done
 * after start-up). This function also sets both the accelerometer and the gyroscope
 * to their most sensitive settings, namely +/- 2g and +/- 250 degrees/sec, and sets
 * the clock source to use the X Gyro for reference, which is slightly better than
 * the default internal clock source.
 */
void MPU6050_Initialize()
{
    MPU6050_SetClockSource(MPU6050_CLOCK_PLL_XGYRO);
    //MPU6050_SetFullScaleGyroRange(MPU6050_GYRO_FS_250);
    MPU6050_SetFullScaleAccelRange(MPU6050_ACCEL_FS_2);
    MPU6050_SetSleepModeStatus(DISABLE);
}

/** Verify the I2C connection.
 * Make sure the device is connected and responds as expected.
 * @return True if connection is valid, FALSE otherwise
 */
bool MPU6050_TestConnection()
{
    return MPU6050_GetDeviceID() == 0x34 ? true : false; //0b110100; 8-bit representation in hex = 0x34
}
// WHO_AM_I register

/** Get Device ID.
 * This register is used to verify the identity of the device (0b110100).
 * @return Device ID (should be 0x68, 104 dec, 150 oct)
 * @see MPU6050_RA_WHO_AM_I
 * @see MPU6050_WHO_AM_I_BIT
 * @see MPU6050_WHO_AM_I_LENGTH
 */
uint8_t MPU6050_GetDeviceID()
{
    uint8_t tmp;
    Sensor_ReadBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_WHO_AM_I, MPU6050_WHO_AM_I_BIT, MPU6050_WHO_AM_I_LENGTH, &tmp);
    return tmp;
}

/** Set clock source setting.
 * An internal 8MHz oscillator, gyroscope based clock, or external sources can
 * be selected as the MPU-60X0 clock source. When the internal 8 MHz oscillator
 * or an external source is chosen as the clock source, the MPU-60X0 can operate
 * in low power modes with the gyroscopes disabled.
 *
 * Upon power up, the MPU-60X0 clock source defaults to the internal oscillator.
 * However, it is highly recommended that the device be configured to use one of
 * the gyroscopes (or an external clock source) as the clock reference for
 * improved stability. The clock source can be selected according to the following table:
 *
 * <pre>
 * CLK_SEL | Clock Source
 * --------+--------------------------------------
 * 0       | Internal oscillator
 * 1       | PLL with X Gyro reference
 * 2       | PLL with Y Gyro reference
 * 3       | PLL with Z Gyro reference
 * 4       | PLL with external 32.768kHz reference
 * 5       | PLL with external 19.2MHz reference
 * 6       | Reserved
 * 7       | Stops the clock and keeps the timing generator in reset
 * </pre>
 *
 * @param source New clock source setting
 * @see MPU6050_GetClockSource()
 * @see MPU6050_RA_PWR_MGMT_1
 * @see MPU6050_PWR1_CLKSEL_BIT
 * @see MPU6050_PWR1_CLKSEL_LENGTH
 */
void MPU6050_SetClockSource(uint8_t source)
{
    Sensor_WriteBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_CLKSEL_BIT, MPU6050_PWR1_CLKSEL_LENGTH, source);
}

/** Set full-scale gyroscope range.
 * @param range New full-scale gyroscope range value
 * @see MPU6050_GetFullScaleGyroRange()
 * @see MPU6050_GYRO_FS_250
 * @see MPU6050_RA_GYRO_CONFIG
 * @see MPU6050_GCONFIG_FS_SEL_BIT
 * @see MPU6050_GCONFIG_FS_SEL_LENGTH
 */
void MPU6050_SetFullScaleGyroRange(uint8_t range)
{
    Sensor_WriteBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_GYRO_CONFIG, MPU6050_GCONFIG_FS_SEL_BIT, MPU6050_GCONFIG_FS_SEL_LENGTH, range);
}

// GYRO_CONFIG register

/** Get full-scale gyroscope range.
 * The FS_SEL parameter allows setting the full-scale range of the gyro sensors,
 * as described in the table below.
 *
 * <pre>
 * 0 = +/- 250 degrees/sec
 * 1 = +/- 500 degrees/sec
 * 2 = +/- 1000 degrees/sec
 * 3 = +/- 2000 degrees/sec
 * </pre>
 *
 * @return Current full-scale gyroscope range setting
 * @see MPU6050_GYRO_FS_250
 * @see MPU6050_RA_GYRO_CONFIG
 * @see MPU6050_GCONFIG_FS_SEL_BIT
 * @see MPU6050_GCONFIG_FS_SEL_LENGTH
 */
uint8_t MPU6050_GetFullScaleGyroRange()
{
    uint8_t tmp;
    Sensor_ReadBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_GYRO_CONFIG, MPU6050_GCONFIG_FS_SEL_BIT, MPU6050_GCONFIG_FS_SEL_LENGTH, &tmp);
    return tmp;
}

/** Get full-scale accelerometer range.
 * The FS_SEL parameter allows setting the full-scale range of the accelerometer
 * sensors, as described in the table below.
 *
 * <pre>
 * 0 = +/- 2g
 * 1 = +/- 4g
 * 2 = +/- 8g
 * 3 = +/- 16g
 * </pre>
 *
 * @return Current full-scale accelerometer range setting
 * @see MPU6050_ACCEL_FS_2
 * @see MPU6050_RA_ACCEL_CONFIG
 * @see MPU6050_ACONFIG_AFS_SEL_BIT
 * @see MPU6050_ACONFIG_AFS_SEL_LENGTH
 */
uint8_t MPU6050_GetFullScaleAccelRange()
{
    uint8_t tmp;
    Sensor_ReadBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_ACCEL_CONFIG, MPU6050_ACONFIG_AFS_SEL_BIT, MPU6050_ACONFIG_AFS_SEL_LENGTH, &tmp);
    return tmp;
}

/** Set full-scale accelerometer range.
 * @param range New full-scale accelerometer range setting
 * @see MPU6050_GetFullScaleAccelRange()
 */
void MPU6050_SetFullScaleAccelRange(uint8_t range)
{
    Sensor_WriteBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_ACCEL_CONFIG, MPU6050_ACONFIG_AFS_SEL_BIT, MPU6050_ACONFIG_AFS_SEL_LENGTH, range);
}

/** Get sleep mode status.
 * Setting the SLEEP bit in the register puts the device into very low power
 * sleep mode. In this mode, only the serial interface and internal registers
 * remain active, allowing for a very low standby current. Clearing this bit
 * puts the device back into normal mode. To save power, the individual standby
 * selections for each of the gyros should be used if any gyro axis is not used
 * by the application.
 * @return Current sleep mode enabled status
 * @see MPU6050_RA_PWR_MGMT_1
 * @see MPU6050_PWR1_SLEEP_BIT
 */
bool MPU6050_GetSleepModeStatus()
{
    uint8_t tmp;
    Sensor_ReadBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_SLEEP_BIT, &tmp);
    return tmp == 0x00 ? false : true;
}

/** Set sleep mode status.
 * @param enabled New sleep mode enabled status
 * @see MPU6050_GetSleepModeStatus()
 * @see MPU6050_RA_PWR_MGMT_1
 * @see MPU6050_PWR1_SLEEP_BIT
 */
void MPU6050_SetSleepModeStatus(FunctionalState NewState)
{
    Sensor_WriteBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_SLEEP_BIT, NewState);
}

/** Get raw 6-axis motion sensor readings (accel/gyro).
 * Retrieves all currently available motion sensor values.
 * @param AccelGyro 16-bit signed integer array of length 6
 * @see MPU6050_RA_ACCEL_XOUT_H
 */
void MPU6050_GetRawAccelGyro(s16* AccelGyro)
{
    u8 tmpBuffer[14];
    u8 i = 0;
    Sensor_I2C_BufferRead(MPU6050_DEFAULT_ADDRESS, tmpBuffer, MPU6050_RA_ACCEL_XOUT_H, 14);
    /* Get acceleration */
    for (i = 0; i < 3; i++)
        AccelGyro[i] = ((s16) ((u16) tmpBuffer[2 * i] << 8) + tmpBuffer[2 * i + 1]);
    /* Get Angular rate */
    for (i = 4; i < 7; i++)
        AccelGyro[i - 1] = ((s16) ((u16) tmpBuffer[2 * i] << 8) + tmpBuffer[2 * i + 1]);

}


/**
 * @}
 *//* end of group MPU6050_Library */
