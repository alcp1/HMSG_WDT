//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 21/May/2026 |                               | ALCP             //
// - Initial Version.                                                         //
//----------------------------------------------------------------------------//

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "debug.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * Debug Initialization
 */
int debug_init(void)
{
    //return  EXIT_SUCCESS / EXIT_FAILURE
    return  EXIT_SUCCESS;
}

/**
 * Debug End
 */
int debug_end(void)
{
    //return  EXIT_SUCCESS / EXIT_FAILURE
    return  EXIT_SUCCESS;
}

/**
 * Print Debug Message
 */
void debug_print(const char * format, ...)
{
    char buff[20];
    struct tm *sTm;
    
    #ifdef DEBUG_ON    
    // Get Timestamp
    time_t now = time(0);
    sTm = gmtime (&now);
    strftime (buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", sTm);    
    
    // Print
    va_list args;
    va_start (args, format);
    printf ("%s: ", buff);
    vprintf (format, args);
    va_end (args);
    
    // Will now print everything in the stdout buffer
    fflush(stdout);
    #endif
}