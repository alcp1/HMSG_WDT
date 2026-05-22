#ifndef DEBUG_H
//#define DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
    
//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
/* Debug */ 
#define DEBUG_ON
#define DEBUG_VERSION

/* MANAGER */
#define DEBUG_MANAGER_ERRORS

/* LED */
#define DEBUG_LED_ERRORS

/* I2C */
#define DEBUG_I2C_ERRORS
    
//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL CONSTANTS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Debug Initialization
 * 
 * \param   None
 * \return  EXIT_SUCCESS / EXIT_FAILURE
 */
int debug_init(void);

/**
 * Debug End
 * 
 * \param   None
 * \return  EXIT_SUCCESS / EXIT_FAILURE
 */
int debug_end(void);

/**
 * Debug Print
 * 
 * \param   same as printf
 * \return  none
 */
void debug_print(const char * format, ...);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_H */

