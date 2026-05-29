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
#include "i2c.h"
#include "led.h"
#include "manager.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define NUMBER_OF_THREADS   3
// WDT Parameters
#define POWER_OFF_DELAY     30      // 30 seconds
#define RPI_WDT_ENABLE      0x63    // Enable WDT
#define RESET_TIMER_LIMIT   40      // 40 seconds
// ATTiny LED Parameters
#define LED_ACTIVE_DUTY     (50*255U/100)   // 50%
#define LED_ACTIVE_ON       (100U/20)       // 100ms
#define LED_ACTIVE_PERIOD   (2000U/20)      // 2seconds
#define LED_TRANSM_DUTY     (75*255U/100)   // 75%
#define LED_TRANSM_ON       (60U/20)        // 60ms
#define LED_TRANSM_PERIOD   (40U/20)        // 40ms
// WDT Commands
#define RPI_COMM_RESET_TIMER    0x01
#define RPI_COMM_REBOOT_ATTINY  0x11
#define RPI_COMM_POWER_CYCLE    0x21

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//
typedef void* (*vp_thread_t)( void *arg );
typedef enum
{
    LED_INIT = 0,
    LED_RUNNING
} manager_led_state_t;
typedef enum
{
    I2C_INIT = 0,
    I2C_READ_CONFIG,
    I2C_WRITE_CONFIG,
    I2C_RUNNING
} manager_i2c_state_t;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS - THREADS
//----------------------------------------------------------------------------//
void* managerHandleWDT01(void *arg);
void* managerHandleWDT02(void *arg);
void* managerHandleWDT03(void *arg);
void managerLED20ms(void);
void managerI2CCheck(void);
bool managerIsWDTConfigured(void);

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
pthread_t pt_threadID[NUMBER_OF_THREADS];
vp_thread_t vp_thread[NUMBER_OF_THREADS] = 
{   managerHandleWDT01, 
    managerHandleWDT02,
    managerHandleWDT03
};
static manager_led_state_t g_led_state;
static manager_i2c_state_t g_i2c_state;
static i2cRegisters g_i2c_dataIN;
static i2cRegisters g_i2c_dataOUT;
static uint8_t g_rPiCommand;

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

// Handle LED every 20ms
void managerLED20ms(void)
{
    switch(g_led_state)
    {
        case LED_INIT:
            led_init();
            g_led_state = LED_RUNNING;
            break;
        case LED_RUNNING:
            led_periodic();
            break;
        default:
            g_led_state = LED_INIT;
            break;
    }
}

// Handle I2C
void managerI2CCheck(void)
{
    i2c_status_t status;
    bool check;
    switch(g_i2c_state)
    {        
        case I2C_INIT:
            i2c_init();
            break;
        case I2C_READ_CONFIG:
            // Set a request (if a request is still pending, a new request 
            // is ignored)
            i2c_startRead(0x00, g_i2c_dataIN.bytes, I2C_REG_ADDR_SIZE);
            status = i2c_rwStatus();
            switch(status)
            {
                case I2C_STATUS_PENDING:
                    // Do nothing - Wait (I2C has internal timeout)
                    break;
                case I2C_STATUS_OK:
                    // Read OK - Check config
                    check = managerIsWDTConfigured();
                    if(!check)
                    {
                        // Write correct configuration
                        g_i2c_state = I2C_WRITE_CONFIG;
                    }
                    else
                    {
                        // Already configured
                        g_i2c_state = I2C_RUNNING;
                    }
                    break;
                case I2C_STATUS_ERROR:
                    // Error - Restart
                    g_i2c_state = I2C_INIT;
                    break;
            }
            break;
        case I2C_WRITE_CONFIG:
            // Set a request (if a request is still pending, a new request 
            // is ignored)
            i2c_startWrite(0x00, g_i2c_dataOUT.bytes, I2C_REG_ADDR_SIZE);
            status = i2c_rwStatus();
            switch(status)
            {
                case I2C_STATUS_PENDING:
                    // Do nothing - Wait (I2C has internal timeout)
                    break;
                case I2C_STATUS_OK:
                    // Write OK: Read config again to check if it is set 
                    // properly
                    g_i2c_state = I2C_READ_CONFIG;
                    break;
                case I2C_STATUS_ERROR:
                    // Error - Restart
                    g_i2c_state = I2C_INIT;
                    break;
            }
            break;
        case I2C_RUNNING:
            // Reset WDT

            break;
        default:
            g_led_state = LED_INIT;
            break;
    }
}

// Check parameters and set the ones to be updated
bool managerIsWDTConfigured(void)
{
    bool ret = true;
    //-----------------------------------------
    // Copy IN to OUT
    //-----------------------------------------
    memcpy(g_i2c_dataOUT.bytes, g_i2c_dataIN.bytes, 
        sizeof(g_i2c_dataOUT.bytes));
    //-----------------------------------------
    // Check Fields
    //-----------------------------------------
    // rPiPCDelay
    g_i2c_dataOUT.fields.rPiPCDelay = POWER_OFF_DELAY;
    if(g_i2c_dataIN.fields.rPiPCDelay != POWER_OFF_DELAY)
    {
        ret = false;
    }
    // rPiWDTEnable
    g_i2c_dataOUT.fields.rPiWDTEnable = RPI_WDT_ENABLE;
    if(g_i2c_dataIN.fields.rPiWDTEnable != RPI_WDT_ENABLE)
    {
        ret = false;
    }
    // resetTimerLimit
    g_i2c_dataOUT.fields.resetTimerLimit = RESET_TIMER_LIMIT;
    if(g_i2c_dataIN.fields.resetTimerLimit != RESET_TIMER_LIMIT)
    {
        ret = false;
    }
    // ledConfig
    g_i2c_dataOUT.fields.ledConfig.activeDuty = LED_ACTIVE_DUTY;
    g_i2c_dataOUT.fields.ledConfig.activeTimeON = LED_ACTIVE_ON;
    g_i2c_dataOUT.fields.ledConfig.activeTimePER = LED_ACTIVE_PERIOD;
    g_i2c_dataOUT.fields.ledConfig.transmitI2CDuty = LED_TRANSM_DUTY;
    g_i2c_dataOUT.fields.ledConfig.transmitI2CTimeON = LED_TRANSM_ON;
    g_i2c_dataOUT.fields.ledConfig.transmitI2CTimePER = LED_TRANSM_PERIOD;
    if(g_i2c_dataIN.fields.ledConfig.activeDuty != LED_ACTIVE_DUTY ||
        g_i2c_dataIN.fields.ledConfig.activeTimeON != LED_ACTIVE_ON ||
        g_i2c_dataIN.fields.ledConfig.activeTimePER != LED_ACTIVE_PERIOD ||
        g_i2c_dataIN.fields.ledConfig.transmitI2CDuty != LED_TRANSM_DUTY ||
        g_i2c_dataIN.fields.ledConfig.transmitI2CTimeON != LED_TRANSM_ON ||
        g_i2c_dataIN.fields.ledConfig.transmitI2CTimePER != LED_TRANSM_PERIOD)
    {
        ret = false;
    }
    // Return
    return ret;
}

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS - THREADS
//----------------------------------------------------------------------------//
/* THREAD - 1 second */
void* managerHandleWDT01(void *arg)
{
    while(1)
    {        
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
        managerLED20ms();        
        //-------------------------------------
        // Handle I2C - 100ms
        //-------------------------------------
        counter100ms++;
        if(counter100ms >= 5)
        {
            // Reset Counter
            counter100ms = 0;
            // Run I2C check
            managerI2CCheck();
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
    // Nothing to be started here

    //--------------------------------------------------------------------------
    // INIT VARIABLES
    //--------------------------------------------------------------------------
    g_led_state = LED_INIT;
    g_i2c_state = I2C_INIT;
    memset(g_i2c_dataIN.bytes, 0, sizeof(g_i2c_dataIN.bytes));
    memset(g_i2c_dataOUT.bytes, 0, sizeof(g_i2c_dataOUT.bytes));
    g_rPiCommand = RPI_COMM_RESET_TIMER;

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
