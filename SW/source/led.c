/*
 * Includes
 */
#include "led.h"
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
// PWM Path
#define PWM_EXPORT_PATH "/sys/class/pwm/pwmchip0/export"
#define PWM_PERIOD_PATH "/sys/class/pwm/pwmchip0/pwm0/period"
#define PWM_DUTY_PATH   "/sys/class/pwm/pwmchip0/pwm0/duty_cycle"
#define PWM_ENABLE_PATH "/sys/class/pwm/pwmchip0/pwm0/enable"
// PWM - SYSFS
#define LED_SYS_PWM_CHANNEL     "0" // PWM0 - GPIO12 - LED 2
#define LED_SYS_PERIOD_STR      "1000000" // 1kHz (Freq. in ns)
#define LED_SYS_PERIOD_NS       1000000U
#define LED_SYS_DUTY_0_STR      "0" // 0%
#define LED_SYS_DUTY_100_STR    "1000000" // 100%
#define LED_SYS_ENABLE          "1" // Enable
// PWM - INTERNAL
#define LED_DUTY_0              0 // 0%
#define LED_DUTY_100            1000U // 100%
// STARTUP STATE TIMING
#define LED_STARTUP_EXPORT_DELAY    (300U/20) // 300ms @ 20ms period
// INIT STATE TIMING
#define LED_INIT_TIME               (1700U/20) // 1,7s @ 20ms period
// ACTIVE STATE CONFIG
#define LED_ACTIVE_DUTY_INIT        (LED_DUTY_100*50/100) // 50%
#define LED_ACTIVE_TON_INIT         (60U/20) // 60ms @ 20ms period
#define LED_ACTIVE_TPER_INIT        (2000U/20) // 2s @ 20ms period
// TRANSMISSTION SIGNALING CONFIG - I2C
#define LED_I2C_TRANSM_DUTY_INIT    (LED_DUTY_100*75/100) // 75%
#define LED_I2C_TRANSM_TON_INIT     (60U/20) // 60ms @ 20ms period
#define LED_I2C_TRANSM_TPER_INIT    (40U/20) // 40ms @ 20ms period
// TRANSMISSTION SIGNALING CONFIG - HMSG
#define LED_HMSG_TRANSM_DUTY_INIT   (LED_DUTY_100*25/100) // 25%
#define LED_HMSG_TRANSM_TON_INIT    (60U/20) // 60ms @ 20ms period
#define LED_HMSG_TRANSM_TPER_INIT   (40U/20) // 40ms @ 20ms period
// STATE
enum
{
    LED_STATE_STARTUP = 0,
    LED_STATE_ACTIVATING,
    LED_STATE_ACTIVE
};

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
// State
static uint8_t g_state;
// Startup timeout
static uint16_t g_ledStartupTimer;
// Transmit Signaling request
static bool g_transmitRequested;
static led_trans_t g_signalingType;
// Active Timers
static uint16_t g_activeTotalTime;
static uint16_t g_transmitTotalTime;
// Transmit Config
static uint16_t g_transmitTimeDuty;
static uint16_t g_transmitTimeON;
static uint16_t g_transmitTimePER;  
// Duty
static uint16_t g_duty;
static uint16_t g_lastDuty;
// Config
static ledConfig g_ledConfig;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
void write_sysfs(const char *file_path, const char *value);
static bool startupLEDPWM(void);
static void setLEDPWM(uint16_t g_duty);

// Auxiliary function to write a string to a sysfs file
void write_sysfs(const char *file_path, const char *value) 
{
    // Open as Write-Only for PWM
    int fd = open(file_path, O_WRONLY);
    if (fd == -1) 
    {
        #ifdef DEBUG_LED_ERRORS
        debug_print("LED: write_sysfs open error on path = %s!\n", file_path);
        #endif
    }
    else
    {
        write(fd, value, strlen(value));
        close(fd);
    }    
}

// Startup PWM output for LED 2 - PWM0 / GPIO12
// Return true when finished
static bool startupLEDPWM(void)
{
    bool ret = false;
    if(g_ledStartupTimer == 0)
    {
        //------------------------------------------------
        // 1. Export the PWM channel 0 (GPIO12 - LED 2)
        //------------------------------------------------
        write_sysfs(PWM_EXPORT_PATH, LED_SYS_PWM_CHANNEL);
        g_ledStartupTimer++;
    }
    else if(g_ledStartupTimer <= LED_STARTUP_EXPORT_DELAY)
    {
        // Increment timer and wait to create the directories 
        g_ledStartupTimer++;
    }
    // Check if timer expired
    if(g_ledStartupTimer > LED_STARTUP_EXPORT_DELAY)
    {
        //------------------------------------------------
        // 2. Set Period (Frequency) in nanoseconds
        //------------------------------------------------        
        write_sysfs(PWM_PERIOD_PATH, LED_SYS_PERIOD_STR);
        //------------------------------------------------
        // 3. Set Duty Cycle in nanoseconds
        //------------------------------------------------
        write_sysfs(PWM_DUTY_PATH, LED_SYS_DUTY_0_STR);
        //------------------------------------------------
        // 4. Enable the outputs
        //------------------------------------------------
        write_sysfs(PWM_ENABLE_PATH, LED_SYS_ENABLE);
        // Finished - return true
        ret = true;        
    }
    return ret;
}

// Set PWM output for LED 2 - PWM0 / GPIO12
static void setLEDPWM(uint16_t g_duty)
{
    char buffer[32];
    uint32_t duty_ns;
    // Calculate the duty cycle as a period (LED_SYS_PERIOD_NS = 100%)
    duty_ns = (uint32_t)(((uint32_t)g_duty * LED_SYS_PERIOD_NS)/LED_DUTY_100);
    // Set buffer with duty cycle as a string
    snprintf(buffer, sizeof(buffer), "%" PRIu32, duty_ns);
    // Write to PWM
    write_sysfs(PWM_DUTY_PATH, buffer);
}


//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/* LED Initialization */
void led_init(void)
{
    //------------------------
    // Init variables
    //------------------------
    // Initial State (when module starts, activating PWM)
    g_state = LED_STATE_STARTUP;
    g_ledStartupTimer = 0;
    // Initial active config
    g_ledConfig.activeTimeON = LED_ACTIVE_TON_INIT;
    g_ledConfig.activeTimePER = LED_ACTIVE_TPER_INIT;
    g_ledConfig.activeDuty = LED_ACTIVE_DUTY_INIT;
    // Initial transmit config
    g_ledConfig.transmitI2CDuty = LED_I2C_TRANSM_DUTY_INIT;
    g_ledConfig.transmitI2CTimeON = LED_I2C_TRANSM_TON_INIT;
    g_ledConfig.transmitI2CTimePER = LED_I2C_TRANSM_TPER_INIT;
    g_ledConfig.transmitHMSGDuty = LED_HMSG_TRANSM_DUTY_INIT;
    g_ledConfig.transmitHMSGTimeON = LED_HMSG_TRANSM_TON_INIT;
    g_ledConfig.transmitHMSGTimePER = LED_HMSG_TRANSM_TPER_INIT;
    g_transmitTimeDuty = g_ledConfig.transmitI2CDuty;
    g_transmitTimeON = g_ledConfig.transmitI2CTimeON;
    g_transmitTimePER = g_ledConfig.transmitI2CTimePER;
    // Set duty to 0%
    g_duty = LED_DUTY_0;
    g_lastDuty = g_duty;
    // Set total time to 0
    g_activeTotalTime = 0;
    g_transmitTotalTime = 0;
    // Clear transmit request
    g_transmitRequested = false;
    g_signalingType = LED_TRANS_SIG_I2C;
    // Init LED with 0% duty
    setLEDPWM(g_duty);
}

/* Periodic function */
void LED_SYS_PERIOD_STRic(void)
{
    uint8_t duty_increment;      
    // Check mode set
    switch(g_state)
    {
        //--------------------
        // Wait until PWM is activated
        //--------------------
        case LED_STATE_STARTUP:
            if(startupLEDPWM())
            {
                // Finished
                g_state = LED_STATE_ACTIVATING;
            }
            break;
        //--------------------
        // Increase duty cycle up to 100% during ACTIVATING state
        //--------------------
        case LED_STATE_ACTIVATING: 
            g_activeTotalTime++;
            if(g_activeTotalTime > LED_INIT_TIME)
            {
                // End of INIT state
                g_state = LED_STATE_ACTIVE;
                // Set total time to 0
                g_activeTotalTime = 0;
            }
            else if(g_activeTotalTime == LED_INIT_TIME)
            {
                // Duty = 100% = 255
                g_duty = LED_DUTY_100;
            }
            else
            {
                // Duty = linear increase
                duty_increment = (LED_DUTY_100 - g_duty);
                duty_increment = duty_increment / 
                    (LED_INIT_TIME - g_activeTotalTime);
                g_duty += duty_increment;
            }
            break;        
        //--------------------
        // Time OFF, time ON at specific duty cycle on active modules.
        // If a trasmit signaling was requestes, use transmit values for Duty,
        // ON and OFF without disrupting the Active ON / OFF timers 
        //--------------------
        case LED_STATE_ACTIVE:
            //--------------------------------------------------------
            // ACTIVE LED SIGNALING - Active all the time after init
            //--------------------------------------------------------
            // Update total time
            g_activeTotalTime++;
            // Check TON
            if(g_activeTotalTime >= g_ledConfig.activeTimeON)
            {
                // Duty = OFF (after Time ON)
                g_duty = LED_DUTY_0;
            }
            else
            {
                // Duty = configured value
                g_duty = g_ledConfig.activeDuty;
            }
            // Check total time
            if(g_activeTotalTime >= g_ledConfig.activeTimePER)
            {
                // Restart Cycle
                g_activeTotalTime = 0;
                // Duty = configured value
                g_duty = g_ledConfig.activeDuty;
            }
            //--------------------------------------------------------
            // TRANSMIT LED SIGNALING - Active when there is a transmit 
            // event after init
            // TRANSMIT has priority over ACTIVE
            //--------------------------------------------------------
            if(g_transmitRequested)
            {
                // Update total time
                g_transmitTotalTime++;
                // Check TON
                if(g_transmitTotalTime >= g_transmitTimeON)
                {
                    // Duty = OFF (after Time ON)
                    g_duty = LED_DUTY_0;
                }
                else
                {
                    // Duty = configured value
                    g_duty = g_transmitTimeDuty;
                }
                // Check total time
                if(g_transmitTotalTime >= g_transmitTimePER)
                {
                    // End of transmit signaling
                    g_transmitTotalTime = 0;
                    g_transmitRequested = false;
                }
            }
            break;
        default:
            led_init();
            break;
    }
    // Update LED output with Duty cycle if new duty was set
    if(g_duty != g_lastDuty)
    {
        setLEDPWM(g_duty);
    }
}

/* Set LED Config */
void led_setLedConfig(ledConfig* config)
{
    // Copy data
    memcpy((void*)&(g_ledConfig),(const void*)(config), sizeof(ledConfig));
}

/* Get LED Config */
void led_getLedConfig(ledConfig* config)
{
    // Copy data
    memcpy((void*)(config),(const void*)&(g_ledConfig), sizeof(ledConfig));
}

/* Request LED Transition signaling */
void led_requestTransmitSignaling(led_trans_t type)
{
    g_transmitRequested = true;
    g_transmitTotalTime = 0;
    switch(type)
    {
        case LED_TRANS_SIG_I2C:
            g_transmitTimeDuty = g_ledConfig.transmitI2CDuty;
            g_transmitTimeON = g_ledConfig.transmitI2CTimeON;
            g_transmitTimePER = g_ledConfig.transmitI2CTimePER; 
            break;
        case LED_TRANS_SIG_HMSG:
            g_transmitTimeDuty = g_ledConfig.transmitHMSGDuty;
            g_transmitTimeON = g_ledConfig.transmitHMSGTimeON;
            g_transmitTimePER = g_ledConfig.transmitHMSGTimePER;
            break;
        default:
            break;
    }
}
