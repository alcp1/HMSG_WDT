#include "i2c.h"
#include "i2c_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
// Timing
#define WDT_CONFIG_REFRESH_TIMEOUT  (60U*1000/100) // 60 seconds
// I2C HOSTA DATA
#define I2C_HOST_DATA_SIZE  I2C_REG_RSTC // HOST will rrite until ledConfig
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

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//
typedef enum
{
    I2C_INIT = 0,
    I2C_READ_CONFIG,
    I2C_WRITE_CONFIG,
    I2C_RUNNING
} app_i2c_state_t;

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
static app_i2c_state_t g_i2c_state;
static i2cRegisters g_i2c_dataIN;
static i2cRegisters g_i2c_dataOUT;
static uint16_t g_refreshConfigCounter;
static uint8_t g_rPiCommand; // Used in multiple threads
static pthread_mutex_t g_app_comm_mutex = PTHREAD_MUTEX_INITIALIZER;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
static bool isWDTConfigured(void);
static void setBufferWDTCommand(void);

// Check parameters and set the ones to be updated
static bool isWDTConfigured(void)
{
    bool ret = true;
    //-----------------------------------------
    // Copy IN to OUT
    //-----------------------------------------
    memcpy(g_i2c_dataOUT.bytes, g_i2c_dataIN.bytes, 
        sizeof(g_i2c_dataOUT.bytes));
    //-----------------------------------------
    // Set command - THREAD PROTECTED
    //-----------------------------------------
    setBufferWDTCommand();
    //-----------------------------------------
    // Check Fields set by HOST
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

// Set command to OUT Buffer - Protected
void setBufferWDTCommand(void)
{
    // LOCK DATA: Protect from other threads running at the same time
    pthread_mutex_lock(&g_app_comm_mutex);
    g_i2c_dataOUT.fields.rPiCommand = g_rPiCommand;
    // UNLOCK DATA
    pthread_mutex_unlock(&g_app_comm_mutex);
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
// Init
void appI2CInit(void)
{    
    // Set buffers
    memset(g_i2c_dataIN.bytes, 0, sizeof(g_i2c_dataIN.bytes));
    memset(g_i2c_dataOUT.bytes, 0, sizeof(g_i2c_dataOUT.bytes));
    // Refresh counter
    g_refreshConfigCounter = 0;
    // Set default command in a thread protected function
    appI2CSetWDTCommand(RPI_COMM_RESET_TIMER);
    // Init I2C
    i2c_init();
    // Set state to read config
    g_i2c_state = I2C_READ_CONFIG;
}

// Handle I2C Config ans state
void appI2CCheck(void)
{
    i2c_status_t status;
    bool check;
    switch(g_i2c_state)
    {        
        case I2C_INIT:
            appI2CInit();
            break;
        case I2C_READ_CONFIG:
            //------------------------------------------------------------------
            // READ ALL REGISTERS
            //------------------------------------------------------------------
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
                    check = isWDTConfigured();
                    if(!check)
                    {
                        // Write correct configuration
                        g_i2c_state = I2C_WRITE_CONFIG;
                    }
                    else
                    {
                        // Already configured
                        g_i2c_state = I2C_RUNNING;
                        // Config just refreshed
                        g_refreshConfigCounter = 0;
                    }
                    break;
                case I2C_STATUS_ERROR:
                    // Error - Restart
                    g_i2c_state = I2C_INIT;
                    break;
            }
            break;
        case I2C_WRITE_CONFIG:
            //------------------------------------------------------------------
            // WRITE ALL REGISTERS SET BY HOST
            //------------------------------------------------------------------
            // Set a request (if a request is still pending, a new request 
            // is ignored)
            i2c_startWrite(0x00, g_i2c_dataOUT.bytes, I2C_HOST_DATA_SIZE);
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
            //------------------------------------------------------------------
            // WRITE COMMAND - rPiCommand
            //------------------------------------------------------------------
            // Set - THREAD PROTECTED
            setBufferWDTCommand();
            // Set a request (if a request is still pending, a new request 
            // is ignored)
            i2c_startWrite(0x00, g_i2c_dataOUT.bytes, 1);
            status = i2c_rwStatus();
            switch(status)
            {
                case I2C_STATUS_PENDING:
                    // Do nothing - Wait (I2C has internal timeout)
                    break;
                case I2C_STATUS_OK:
                    // Write OK: Do nothing until refresh
                    g_refreshConfigCounter++;
                    if(g_refreshConfigCounter >= WDT_CONFIG_REFRESH_TIMEOUT)
                    {
                        // Refresh config
                        g_i2c_state = I2C_READ_CONFIG;
                    }
                    break;
                case I2C_STATUS_ERROR:
                    // Error - Restart
                    g_i2c_state = I2C_INIT;
                    break;
            }
            break;
        default:
            g_i2c_state = I2C_INIT;
            break;
    }
}

/* Set the WDT Command*/
void appI2CSetWDTCommand(wdt_command_t command)
{
    // LOCK DATA: Protect from other threads running at the same time
    pthread_mutex_lock(&g_app_comm_mutex);
    g_rPiCommand = command;
    // UNLOCK DATA
    pthread_mutex_unlock(&g_app_comm_mutex);
}