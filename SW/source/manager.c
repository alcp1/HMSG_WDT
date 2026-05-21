//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 21/May/2026 |                               | ALCP             //
// - Initial Version.                                                         //
//----------------------------------------------------------------------------//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include "app.h"
#include "debug.h"
#include "manager.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define NUMBER_OF_THREADS   2

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//
typedef void* (*vp_thread_t)( void *arg );

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS - THREADS
//----------------------------------------------------------------------------//
void* managerHandleWDT01(void *arg);
void* managerHandleWDT02(void *arg);

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
pthread_t pt_threadID[NUMBER_OF_THREADS];
vp_thread_t vp_thread[NUMBER_OF_THREADS] = 
{   managerHandleWDT01, 
    managerHandleWDT02};

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS - AUXILIARY
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS - THREADS
//----------------------------------------------------------------------------//
/* THREAD - 1 second */
void* managerHandleWDT01(void *arg)
{        
    while(1)
    {        
        // 1s loop
        sleep(1);
    }
}
/* THREAD - 50ms */
void* managerHandleWDT02(void *arg)
{        
    while(1)
    {        
        // 50ms loop
        usleep(50000);
    }
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* Init */
void managerInit(void)
{    
    int li_index;
    int li_check;

    /**************************************************************************
     * Build date
     *************************************************************************/
    #ifdef DEBUG_VERSION
    debug_print("HMSG WDT Start! Version = %s.%s\n", APP_SW_MAIN_VERSION, 
            APP_SW_SUB_VERSION);
    debug_print("HMSG WDT Build date/time = %s - %s\n", __DATE__, __TIME__);
    #endif    
    /**************************************************************************
     * INIT THREADS - CREATE AND JOIN
     *************************************************************************/    
    // Create Threads
    for(li_index = 0; li_index < NUMBER_OF_THREADS; li_index++)
    {
        li_check = pthread_create(&pt_threadID[li_index], 
                NULL, vp_thread[li_index], NULL);
        if(li_check)
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_MANAGER_ERRORS
            debug_print("MANAGER: THREAD CREATE ERROR!\n");
            debug_print("- Thread Index = %d\n", li_index);
            #endif
        }
    }    
    // Join Threads
    for(li_index = 0; li_index < NUMBER_OF_THREADS; li_index++)
    {
        li_check = pthread_join(pt_threadID[li_index], NULL);
        if(li_check)
        {
            /***************/
            /* FATAL ERROR */
            /***************/
            #ifdef DEBUG_MANAGER_ERRORS
            debug_print("MANAGER: THREAD JOIN ERROR!\n");
            debug_print("- Thread Index = %d\n", li_index);
            #endif
        }
    }
}
