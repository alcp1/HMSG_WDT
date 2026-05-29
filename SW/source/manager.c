#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "app.h"
#include "debug.h"
#include "led.h"
#include "i2c.h"
#include "i2c_app.h"
#include "manager.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define NUMBER_OF_THREADS   3

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//
typedef void* (*vp_thread_t)( void *arg );

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS - THREADS
//----------------------------------------------------------------------------//
void* managerHandleWDT01(void *arg);
void* managerHandleWDT02(void *arg);
void* managerHandleWDT03(void *arg);

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
pthread_t pt_threadID[NUMBER_OF_THREADS];
vp_thread_t vp_thread[NUMBER_OF_THREADS] = 
{   managerHandleWDT01, 
    managerHandleWDT02,
    managerHandleWDT03
};

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS - AUXILIARY
//----------------------------------------------------------------------------//
// Sleep 1s - Can only be called by a single thread
void managerDelayWDT01(void)
{
    // 1 second sleep
    struct timespec req = { .tv_sec = 1, .tv_nsec = 0 };
    struct timespec rem;
    int ret;
    // 1s loop
    while((ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem)) != 0)
    {
        if (ret == EINTR) 
        {
            // Interrupted by a signal:
            // update 'req' to sleep for the remaining time
            req = rem; 
        }
        else
        {
            #ifdef DEBUG_MANAGER_ERRORS
            debug_print("MANAGER: managerDelayWDT01 Error = %d!\n", ret);
            #endif
            break;
        }
    }
}

// Sleep 20ms - Can only be called by a single thread
void managerDelayWDT02(void)
{
    // 20ms sleep
    struct timespec req = { .tv_sec = 0, .tv_nsec = 20000000 };
    struct timespec rem;
    int ret;
    // 1s loop
    while((ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem)) != 0)
    {
        if (ret == EINTR) 
        {
            // Interrupted by a signal:
            // update 'req' to sleep for the remaining time
            req = rem; 
        }
        else
        {
            #ifdef DEBUG_MANAGER_ERRORS
            debug_print("MANAGER: managerDelayWDT02 Error = %d!\n", ret);
            #endif
            break;
        }
    }
}

// Sleep 10ms - Can only be called by a single thread
void managerDelayWDT03(void)
{
    // 10ms sleep
    struct timespec req = { .tv_sec = 0, .tv_nsec = 10000000 };
    struct timespec rem;
    int ret;
    // 1s loop
    while((ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem)) != 0)
    {
        if (ret == EINTR) 
        {
            // Interrupted by a signal:
            // update 'req' to sleep for the remaining time
            req = rem; 
        }
        else
        {
            #ifdef DEBUG_MANAGER_ERRORS
            debug_print("MANAGER: managerDelayWDT03 Error = %d!\n", ret);
            #endif
            break;
        }
    }
}

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS - THREADS
//----------------------------------------------------------------------------//
/* THREAD - 1 second */
void* managerHandleWDT01(void *arg)
{
    while(1)
    {        
        // Set External Watchdog Command
        appI2CSetWDTCommand(RPI_COMM_RESET_TIMER);
        // Sleep 1s
        managerDelayWDT01();
    }
}

/* THREAD - 20ms */
void* managerHandleWDT02(void *arg)
{    
    uint8_t counter100ms;
    counter100ms = 0;
    while(1)
    {
        //-------------------------------------
        // Handle LED - 20ms
        //-------------------------------------
        led_periodic();
        //-------------------------------------
        // Handle I2C - 100ms
        //-------------------------------------
        counter100ms++;
        if(counter100ms >= 5)
        {
            // Reset Counter
            counter100ms = 0;
            // Run I2C check every 100ms
            appI2CCheck();
        }        
        //-------------------------------------
        // Sleep 20ms
        //-------------------------------------
        managerDelayWDT02();
    }
}

/* THREAD - 10ms */
void* managerHandleWDT03(void *arg)
{
    while(1)
    {
        //-------------------------------------
        // I2C I/O: periodic read / write
        //-------------------------------------
        i2c_periodic();
        //-------------------------------------
        // Sleep 10ms
        //-------------------------------------
        managerDelayWDT03();
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

    //--------------------------------------------------------------------------
    // Build date
    //--------------------------------------------------------------------------
    #ifdef DEBUG_VERSION
    debug_print("HMSG WDT Start! Version = %s.%s\n", APP_SW_MAIN_VERSION, 
            APP_SW_SUB_VERSION);
    debug_print("HMSG WDT Build date/time = %s - %s\n", __DATE__, __TIME__);
    #endif
    //--------------------------------------------------------------------------
    // INIT MODULES
    //--------------------------------------------------------------------------
    // I2C App
    appI2CInit();
    led_init();

    //--------------------------------------------------------------------------
    // INIT VARIABLES
    //--------------------------------------------------------------------------  

    //--------------------------------------------------------------------------
    // INIT THREADS - CREATE AND JOIN
    //--------------------------------------------------------------------------
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
