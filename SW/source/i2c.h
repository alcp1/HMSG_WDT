#ifndef I2C_H
#define I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "led.h"

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define I2C_REG_ADDR_SIZE   22
#define I2C_REG_RPICOM      0   // rPiCommand
#define I2C_REG_RPIDLY      1   // rPiPCDelay
#define I2C_REG_WDTEN       2   // rPiWDTEnable
#define I2C_REG_TIM         3   // resetTimer
#define I2C_REG_TIML        5   // resetTimerLimit
#define I2C_REG_RCNT        7   // resetCounter
#define I2C_REG_LEDC        9   // ledConfig
#define I2C_REG_RSTC        15  // resetCause
#define I2C_REG_ADCR        16  // adcReading
#define I2C_REG_SOFF        18  // sigrow_offset
#define I2C_REG_SGAI        19  // sigrow_gain
#define I2C_REG_VER         20  // version

//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//
typedef union 
{
    // Raw unsigned byte array
    uint8_t bytes[I2C_REG_ADDR_SIZE];
    // Individual Fields
    struct 
    {
        uint8_t rPiCommand;         // byte index 0
        uint8_t rPiPCDelay;         // byte index 1
        uint8_t rPiWDTEnable;       // byte index 2
        uint16_t resetTimer;        // byte index 3 and 4
        uint16_t resetTimerLimit;   // byte index 5 and 6
        uint16_t resetCounter;      // byte index 7 and 8
        ledConfig ledConfig;        // byte index 9 to 14
        uint8_t resetCause;         // byte index 15
        int16_t adcReading;         // byte index 16 and 17
        int8_t sigrow_offset;       // byte index 18
        uint8_t sigrow_gain;        // byte index 19
        uint16_t version;           // byte index 20 and 21
    } fields;
} i2cRegisters;

typedef enum
{
    I2C_STATUS_OK = 0,
    I2C_STATUS_PENDING,
    I2C_STATUS_ERROR,
} i2c_status_t;

//----------------------------------------------------------------------------//
// EXTERNAL VARIABLES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Init I2C.
 * 
 * \param   nothing
 * \return  nothing
 */
extern void i2c_init(void);

/**
 * I2C periodic task.
 * 
 * \param   nothing
 * \return  nothing
 */
extern void i2c_periodic(void);

/**
 * I2C start read (non-blocking)
 * 
 * \param[in]   address First Register Address to be read.
 * \param[out]  buffer  Buffer to be filled with read data.
 * \param[in]   size    number of bytes to be read.
 * \return  if request was successfull
 * \retval  true    read was successfull
 * \retval  false   read was not successfull (it was busy)
 */
extern bool i2c_startRead(uint8_t address, uint8_t *buffer, uint8_t size);

/**
 * I2C start write (non-blocking)
 * 
 * \param[in]   address First Register Address to be written.
 * \param[out]  buffer  Buffer with data to be written.
 * \param[in]   size    number of bytes to be written.
 * \return  if request was successfull
 * \retval  true    write was successfull
 * \retval  false   write was not successfull (it was busy)
 */
extern bool i2c_startWrite(uint8_t address, uint8_t *buffer, uint8_t size);

/**
 * I2C get read / write status
 * 
 * \param   nothing
 * \return  status of the read / write command
 * \retval  I2C_STATUS_OK   read/write successfull
 * \retval  I2C_STATUS_PENDING  read/write still ongoing
 * \retval  I2C_STATUS_ERROR    read/write error
 */
extern i2c_status_t i2c_rwStatus(void);



#ifdef __cplusplus
}
#endif

#endif /* I2C_H */

