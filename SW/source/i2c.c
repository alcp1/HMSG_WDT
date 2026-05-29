#include "i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
#define I2C_BUS "/dev/i2c-1"            // Default I2C bus on Raspberry Pi
#define CLIENT_ADDR 0x1E                // ATTiny402
#define I2C_RETRY_TIMEOUT   (200/10)    // 200ms in 10ms time base

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//
typedef enum
{
    I2C_ST_INIT = 0,
    I2C_ST_WRITE_STARTED,
    I2C_ST_READ_STARTED,
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
    uint8_t retry;
} i2c_control_t;

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
// Buffer: 1 byte for register, followed by I2C data (add + 1 to the size)
static uint8_t g_localBuffer[I2C_REG_ADDR_SIZE + 1];
static i2c_control_t g_controlI2C;
static pthread_mutex_t i2c_data_mutex = PTHREAD_MUTEX_INITIALIZER;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
void copyI2CData(i2c_control_t *dest, const i2c_control_t *src);

void copyI2CData(i2c_control_t *dest, const i2c_control_t *src)
{
    // Copy all, except buffer
    dest->fd = src->fd;
    dest->error = src->error;
    dest->address = src->address;
    dest->size = src->size;
    dest->dataReady = src->dataReady;
    dest->state = src->state;
    dest->isWrite = src->isWrite;
    dest->retry = src->retry;
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
    g_controlI2C.retry = 0;
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
    // Clear buffer
    memset(g_localBuffer, 0, sizeof(g_localBuffer));
}

/* I2C periodic task */
void i2c_periodic(void)
{
    struct i2c_msg i2c_msgs[2];
    struct i2c_rdwr_ioctl_data packets;
    i2c_control_t l_controlI2C;
    int ret;
    // LOCK DATA: Protect from other threads running at the same time
    pthread_mutex_lock(&i2c_data_mutex);
    // Read from global to manipulate data
    copyI2CData(&l_controlI2C, &g_controlI2C);
    // Copy from buffer for write
    if(g_controlI2C.isWrite)
    {
        // Copy starts at byte 1 of destination because byte 0 is the register 
        // address
        memcpy(&g_localBuffer[1], g_controlI2C.buffer, g_controlI2C.size);
    }    
    // UNLOCK DATA
    pthread_mutex_unlock(&i2c_data_mutex);
    // Check state
    switch(l_controlI2C.state)
    {
        case I2C_ST_INIT:
            // Do nothing
            break;
        case I2C_ST_WRITE_STARTED:
            // Set Address (buffer already set above)
            g_localBuffer[0] = l_controlI2C.address;
            //-----------------------------------------------------            
            // Message 1: Write register address and data
            //-----------------------------------------------------
            // Client
            i2c_msgs[0].addr = CLIENT_ADDR;  
            // Write
            i2c_msgs[0].flags = 0;
            // 1 byte register + size
            i2c_msgs[0].len = l_controlI2C.size + 1;   
            // Buffer - local
            i2c_msgs[0].buf = g_localBuffer;
            //-----------------------------------------------------
            // Set packets - only 1 for write
            //-----------------------------------------------------
            packets.msgs = i2c_msgs;
            packets.nmsgs = 1;
            //-----------------------------------------------------
            // Check for retries
            //-----------------------------------------------------
            if(l_controlI2C.retry > 0)
            {
                // Decrement retry
                l_controlI2C.retry--;
                //-----------------------------------------------------
                // Try to send package
                //-----------------------------------------------------
                ret = ioctl(l_controlI2C.fd, I2C_RDWR, &packets);
                if (ret >= 0)
                {
                    // Success (no need to copy buffer for write)
                    l_controlI2C.state = I2C_ST_INIT;
                    l_controlI2C.error = false;
                    l_controlI2C.dataReady = true;                                 
                } 
                else 
                {                    
                    #ifdef DEBUG_I2C_PACKET_ERRORS
                    debug_print("I2C: i2c_periodic ioctl error = %d;" ret);
                    #endif
                }
                break;
            }
            else
            {
                // Retries finished, set error and change to initial state
                l_controlI2C.state = I2C_ST_INIT;
                l_controlI2C.error = true;
                #ifdef DEBUG_I2C_ERRORS
                debug_print("I2C: i2c_periodic write retries error!");
                #endif
            }            
        case I2C_ST_READ_STARTED:
            // Set register address
            g_localBuffer[0] = l_controlI2C.address;
            //-----------------------------------------------------            
            // Message 1: Write register address
            //-----------------------------------------------------
            // Client
            i2c_msgs[0].addr = CLIENT_ADDR;  
            // Write (first we have to write the address to read afterwards)
            i2c_msgs[0].flags = 0;
            // 1 byte register
            i2c_msgs[0].len = 1;
            // Buffer - local
            i2c_msgs[0].buf = &(g_localBuffer[0]);

            //-----------------------------------------------------            
            // Message 2: Read data
            //-----------------------------------------------------           
            // Set Address (buffer already set above)
            g_localBuffer[0] = l_controlI2C.address;
            // Client
            i2c_msgs[1].addr = CLIENT_ADDR;  
            // Read
            i2c_msgs[1].flags = I2C_M_RD;
            // size only for read
            i2c_msgs[1].len = l_controlI2C.size; 
            // Buffer - local (data starts at position 1)
            i2c_msgs[1].buf = &(g_localBuffer[1]);
            // Set packets - 2 for read
            packets.msgs = i2c_msgs;
            packets.nmsgs = 2;
            // Check for retries
            if(l_controlI2C.retry > 0)
            {
                // Decrement retry
                l_controlI2C.retry--;
                // Try to send package
                ret = ioctl(l_controlI2C.fd, I2C_RDWR, &packets);
                if (ret >= 0)
                {
                    // Success
                    l_controlI2C.state = I2C_ST_INIT;
                    l_controlI2C.dataReady = true;                        
                    // LOCK DATA: Protect from other threads
                    pthread_mutex_lock(&i2c_data_mutex);
                    // Copy data to buffer - has to be g_controlI2C
                    memcpy(g_controlI2C.buffer, &g_localBuffer[1], 
                        g_controlI2C.size);
                    // UNLOCK DATA
                    pthread_mutex_unlock(&i2c_data_mutex);
                    // Check state                
                } 
                else 
                {                    
                    #ifdef DEBUG_I2C_PACKET_ERRORS
                    debug_print("I2C: i2c_periodic ioctl error = %d;" ret);
                    #endif
                }
                break;
            }
            else
            {
                // Retries finished, set error and change to initial state
                l_controlI2C.state = I2C_ST_INIT;
                l_controlI2C.error = true;
                #ifdef DEBUG_I2C_ERRORS
                debug_print("I2C: i2c_periodic read retries error!");
                #endif
            }
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
void i2c_startRead(uint8_t address, uint8_t *buffer, uint8_t size)
{    
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
            g_controlI2C.state = I2C_ST_WRITE_STARTED;
            g_controlI2C.isWrite = false;
            g_controlI2C.retry = I2C_RETRY_TIMEOUT;
            // Clear buffer
            memset(g_localBuffer, 0, sizeof(g_localBuffer));
        }
        // UNLOCK DATA
        pthread_mutex_unlock(&i2c_data_mutex);        
    }
}

/* I2C start write */
void i2c_startWrite(uint8_t address, uint8_t *buffer, uint8_t size)
{
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
            g_controlI2C.state = I2C_ST_WRITE_STARTED;
            g_controlI2C.isWrite = true;
            g_controlI2C.retry = I2C_RETRY_TIMEOUT;
            // Clear buffer
            memset(g_localBuffer, 0, sizeof(g_localBuffer));
        }
        // UNLOCK DATA
        pthread_mutex_unlock(&i2c_data_mutex);
    }
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