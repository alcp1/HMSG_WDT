#include "i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include <pthread.h>

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
    int fd;
    bool error;
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
static i2c_control_t g_controlI2C;
static uint8_t g_localBuffer[I2C_REG_ADDR_SIZE];
static pthread_mutex_t i2c_data_mutex = PTHREAD_MUTEX_INITIALIZER;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
void copyI2CData(i2c_control_t *dest, const i2c_control_t *src);

void copyI2CData(i2c_control_t *dest, const i2c_control_t *src)
{
    dest->fd = src->fd;
    dest->error = src->error;
    dest->address = src->address;
    dest->buffer = src->buffer;
    dest->size = src->size;
    dest->dataReady = src->dataReady;
    dest->state = src->state;
    dest->isWrite = src->isWrite;
}

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

/* Initialization */
void i2c_init(void)
{    
    int ret;
    int fd;
    // Init variables
    fd = -1;
    // LOCK DATA: Protect from other threads running at the same time
    pthread_mutex_lock(&i2c_data_mutex);
    g_controlI2C.fd = -1;
    g_controlI2C.error = false;
    g_controlI2C.address = 0;
    g_controlI2C.buffer = NULL;
    g_controlI2C.size = 0;
    g_controlI2C.dataReady = false;
    g_controlI2C.state = I2C_ST_INIT;
    g_controlI2C.isWrite = false;
    // UNLOCK DATA
    pthread_mutex_unlock(&i2c_data_mutex);
    //------------------------------------------
    // 1. Open I2C bus in non-blocking mode
    //------------------------------------------
    ret = open(I2C_BUS, O_RDWR | O_NONBLOCK);
    if(ret < 0)
    {
        //------------------------------------------
        // Set error - open
        //------------------------------------------
        // LOCK DATA: Protect from other threads running at the same time
        pthread_mutex_lock(&i2c_data_mutex);
        g_controlI2C.error = true;
        // UNLOCK DATA
        pthread_mutex_unlock(&i2c_data_mutex);
        #ifdef DEBUG_I2C_ERRORS
        debug_print("I2C: i2c_init error - open  = %d", ret);
        #endif        
    }
    else
    {
        // LOCK DATA: Protect from other threads running at the same time
        pthread_mutex_lock(&i2c_data_mutex);
        g_controlI2C.fd = ret;
        fd = ret;
        // UNLOCK DATA
        pthread_mutex_unlock(&i2c_data_mutex);
    }
    //------------------------------------------
    // 2. Set the slave device address
    //------------------------------------------
    ret = ioctl(fd, I2C_SLAVE, CLIENT_ADDR);
    if(ret < 0)
    {
        // Set error
        // LOCK DATA: Protect from other threads running at the same time
        pthread_mutex_lock(&i2c_data_mutex);
        g_controlI2C.error = true;
        // UNLOCK DATA
        pthread_mutex_unlock(&i2c_data_mutex);        
        #ifdef DEBUG_I2C_ERRORS
        debug_print("I2C: i2c_init error - ioctl  = %d", ret);
        #endif
    }    
}

/* I2C periodic task */
void i2c_periodic(void)
{
    i2c_control_t l_controlI2C;
    // LOCK DATA: Protect from other threads running at the same time
    pthread_mutex_lock(&i2c_data_mutex);
    // Read from global to manipulate data
    copyI2CData(&l_controlI2C, &g_controlI2C);
    // UNLOCK DATA
    pthread_mutex_unlock(&i2c_data_mutex);
    // Check state
    switch(l_controlI2C.state)
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
            l_controlI2C.state = I2C_ST_INIT;
            break;
    }
    // LOCK DATA: Protect from other threads running at the same time
    pthread_mutex_lock(&i2c_data_mutex);
    // Write to global after manipulating data
    copyI2CData(&g_controlI2C, &l_controlI2C);
    // UNLOCK DATA
    pthread_mutex_unlock(&i2c_data_mutex);
}

/* I2C start read */
bool i2c_startRead(uint8_t address, uint8_t *buffer, uint8_t size)
{
    bool ret;
    // init as invalid request (busy or error)
    ret = false;
    // Check for out of bounds
    if( (address + size) > I2C_REG_ADDR_SIZE )
    {
        // Out of bounds
        #ifdef DEBUG_I2C_ERRORS
        debug_print("I2C: i2c_startRead out of bounds error - address  = %d \
            size = %d", address, size);
        #endif
    }
    else
    {
        //--------------------------------------------
        // Start request
        //--------------------------------------------
        // LOCK DATA: Protect from other threads running at the same time
        pthread_mutex_lock(&i2c_data_mutex);
        if(g_controlI2C.state == I2C_ST_INIT)
        {
            g_controlI2C.address = address;
            g_controlI2C.buffer = buffer;
            g_controlI2C.dataReady = false;
            g_controlI2C.size = size;
            g_controlI2C.state = I2C_ST_WRITE_ADDR_STARTED;
            g_controlI2C.isWrite = false;
            // request OK
            ret = true;
        }
        // UNLOCK DATA
        pthread_mutex_unlock(&i2c_data_mutex);        
    }
    return ret;
}

/* I2C start write */
bool i2c_startWrite(uint8_t address, uint8_t *buffer, uint8_t size)
{
    bool ret;
    // init as invalid request (busy or error)
    ret = false;
    // Check for out of bounds
    if( (address + size) > I2C_REG_ADDR_SIZE )
    {
        // Out of bounds
        #ifdef DEBUG_I2C_ERRORS
        debug_print("I2C: i2c_startWrite out of bounds error - address  = %d \
            size = %d", address, size);
        #endif
    }
    else
    {
        //--------------------------------------------
        // Start request
        //--------------------------------------------
        // LOCK DATA: Protect from other threads running at the same time
        pthread_mutex_lock(&i2c_data_mutex);
        if(g_controlI2C.state == I2C_ST_INIT)
        {
            g_controlI2C.address = address;
            g_controlI2C.buffer = buffer;
            g_controlI2C.dataReady = false;
            g_controlI2C.size = size;
            g_controlI2C.state = I2C_ST_WRITE_ADDR_STARTED;
            g_controlI2C.isWrite = true;
            // request OK
            ret = true;
        }
        // UNLOCK DATA
        pthread_mutex_unlock(&i2c_data_mutex);
    }
    return ret;
}

/* I2C get read / write status */
i2c_status_t i2c_rwStatus(void)
{
    i2c_status_t ret;
    // Init as pending
    ret = I2C_STATUS_PENDING;
    // LOCK DATA: Protect from other threads running at the same time
    pthread_mutex_lock(&i2c_data_mutex); 
    // Check state
    if( (g_controlI2C.state == I2C_ST_INIT) && (g_controlI2C.dataReady) &&
        (!g_controlI2C.error) )
    {
        // Finished
        ret = I2C_STATUS_OK;
    }
    if(g_controlI2C.error)
    {
        // Error
        ret = I2C_STATUS_ERROR;
    }
    // UNLOCK DATA
    pthread_mutex_unlock(&i2c_data_mutex);   
    // Return
    return ret;
}