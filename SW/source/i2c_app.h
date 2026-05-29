#ifndef I2C_APP_H
#define I2C_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
// WDT Commands
typedef enum
{
    RPI_COMM_RESET_TIMER = 0x01,
    RPI_COMM_REBOOT_ATTINY = 0x11,
    RPI_COMM_POWER_CYCLE = 0x21
} wdt_command_t;
//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL VARIABLES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Inits I2C App (I2C communication manager).
 * 
 * \param   nothing
 * \return  nothing
 */
extern void appI2CInit(void);

/**
 * Checks the I2C data read from the device against the output I2C data.
 * 
 * \param   nothing
 * \return  nothing
 */
extern void appI2CCheck(void);

/**
 * Sets the I2C command to be sent to the ATTiny module
 * 
 * \param   command
 * \return  nothing
 */
extern void appI2CSetWDTCommand(wdt_command_t command);


#ifdef __cplusplus
}
#endif

#endif /* I2C_APP_H */