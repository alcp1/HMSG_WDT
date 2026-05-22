#include "i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <errno.h>

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define I2C_BUS "/dev/i2c-1"    // Default I2C bus on Raspberry Pi
#define CLIENT_ADDR 0x1E        // ATTiny402

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//
typedef enum
{
    I2C_ST_INIT = 0,
    I2C_ST_WRITE_ADDR_STARTED,
    I2C_ST_WRITE_DATA_STARTED,
    I2C_ST_READ_DATA_STARTED,
} i2c_state_t;

typedef struct
{
    uint8_t address;
    uint8_t* buffer;
    uint8_t size;
    bool dataReady;
    i2c_state_t state;
    bool isWrite;
} i2c_control_t;

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
static int g_fdI2C;
static bool g_errI2C;
static i2c_control_t g_controlI2C;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

/* Initialization */
void i2c_init(void)
{    
    int ret;
    // Init variables
    // fd
    g_fdI2C = -1;
    // Error
    g_errI2C = false; 
    // Control data
    g_controlI2C.address = 0;
    g_controlI2C.buffer = NULL;
    g_controlI2C.dataReady = false;
    g_controlI2C.size = 0;
    g_controlI2C.state = I2C_ST_INIT;
    //------------------------------------------
    // 1. Open I2C bus in non-blocking mode
    //------------------------------------------
    ret = open(I2C_BUS, O_RDWR | O_NONBLOCK);
    if(ret < 0)
    {
        // Set error
        g_errI2C = true;
        #ifdef DEBUG_I2C_ERRORS
        debug_print("I2C: i2c_init error - open  = %d", ret);
        #endif        
    }
    else
    {
        g_fdI2C = ret;
    }
    //------------------------------------------
    // 2. Set the slave device address
    //------------------------------------------
    ret = ioctl(g_fdI2C, I2C_SLAVE, CLIENT_ADDR);
    if(ret < 0)
    {
        // Set error
        g_errI2C = true;
        #ifdef DEBUG_I2C_ERRORS
        debug_print("I2C: i2c_init error - ioctl  = %d", ret);
        #endif
    }    
}

/* I2C periodic task */
void i2c_periodic(void)
{
    // Check state
    switch(g_controlI2C.state)
    {
        case I2C_ST_INIT:
            // Do nothing
            break;
        case I2C_ST_WRITE_ADDR_STARTED:
            break;
        case I2C_ST_WRITE_DATA_STARTED:
            break;
        case I2C_ST_READ_DATA_STARTED:
            break;
        default:
            g_controlI2C.state = I2C_ST_INIT;
            break;
    }
}

/* I2C start read (non-blocking) */
bool i2c_startRead(uint8_t address, uint8_t *buffer, uint8_t size)
{
    bool ret;
    // init as invalid request (busy)
    ret = false;
    if(g_controlI2C.state == I2C_ST_INIT)
    {
        // start request
        g_controlI2C.address = address;
        g_controlI2C.buffer = buffer;
        g_controlI2C.dataReady = false;
        g_controlI2C.size = size;
        g_controlI2C.state = I2C_ST_WRITE_ADDR_STARTED;
        g_controlI2C.isWrite = false;
        // request OK
        ret = true;
    }
    return ret;
}

/* I2C start write (non-blocking) */
bool i2c_startWrite(uint8_t address, uint8_t *buffer, uint8_t size)
{
    bool ret;
    // init as invalid request (busy)
    ret = false;
    if(g_controlI2C.state == I2C_ST_INIT)
    {
        // start request
        g_controlI2C.address = address;
        g_controlI2C.buffer = buffer;
        g_controlI2C.dataReady = false;
        g_controlI2C.size = size;
        g_controlI2C.state = I2C_ST_WRITE_ADDR_STARTED;
        g_controlI2C.isWrite = true;
        // request OK
        ret = true;
    }
    return ret;
}

/* I2C get read / write status */
i2c_status_t i2c_rwStatus(void)
{
    i2c_status_t ret;
    // Init as pending
    ret = I2C_STATUS_PENDING; 
    // Check state
    if( (g_controlI2C.state == I2C_ST_INIT) && (g_controlI2C.dataReady) &&
        (!g_errI2C) )
    {
        // Finished
        ret = I2C_STATUS_OK;
    }
    if(g_errI2C)
    {
        // Error
        ret = I2C_STATUS_ERROR;
    }    
    // Return
    return ret;
}