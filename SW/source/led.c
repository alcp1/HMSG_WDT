//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 21/May/2026 |                               | ALCP             //
// - First version                                                            //
//----------------------------------------------------------------------------//

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "led.h"

//----------------------------------------------------------------------------//
// INTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
// INIT STATE TIMING
#define LED_INIT_TIME           (1700U/20)          // 1,7s @ 20ms period
#define LED_INIT_DUTY_INCREMENT (255U/(1700U/20))   // 1,7s @ 20ms period
// ACTIVE STATE INITIAL CONFIG
#define LED_ACTIVE_DUTY_INIT    (127)               // 50%
#define LED_ACTIVE_TON_INIT     (40U/20)            // 40ms @ 20ms period
#define LED_ACTIVE_TPER_INIT    (460U/20)           // 460ms @ 20ms period
// TRASNMISSTION SIGNALING INITIAL CONFIG
#define LED_TRANSM_DUTY_INIT    (191)               // 75%
#define LED_TRANSM_TON_INIT     (60U/20)            // 60ms @ 20ms period
#define LED_TRANSM_TPER_INIT    (40U/20)            // 40ms @ 20ms period
// STATE
enum
{
    LED_STATE_INIT = 0,
    LED_STATE_ACTIVE
};

//----------------------------------------------------------------------------//
// INTERNAL TYPES
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// INTERNAL GLOBAL VARIABLES
//----------------------------------------------------------------------------//
// State
static volatile uint8_t g_state;
// Transmit Signaling request
static volatile bool g_transmitRequested;
// Active Timers
static volatile uint8_t g_activeTotalTime;
static volatile uint8_t g_transmitTotalTime;
// Duty
static volatile uint8_t g_duty8;   // 0 = 0%, 255 = 100%
static volatile uint16_t g_duty16;
// Config
static volatile ledConfig g_ledConfig;

//----------------------------------------------------------------------------//
// INTERNAL FUNCTIONS
//----------------------------------------------------------------------------//


//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//

/* LED Initialization */
void led_init(void)
{
    //------------------------
    // Init variables
    //------------------------
    // Initial State (when module starts, initial transition on LED)
    g_state = LED_STATE_INIT;
    // Initial active config
    g_ledConfig.activeTimeON = LED_ACTIVE_TON_INIT;
    g_ledConfig.activeTimePER = LED_ACTIVE_TPER_INIT;
    g_ledConfig.activeDuty = LED_ACTIVE_DUTY_INIT;
    // Set duty to 0%
    g_duty8 = 0;
    g_duty16 = 0;
    // Set total time to 0
    g_activeTotalTime = 0;
    g_transmitTotalTime = 0;
    // Clear transmit request
    g_transmitRequested = false;
}

/* Periodic function */
void led_periodic(void)
{
    // Check mode set
    switch(g_state)
    {
        //--------------------
        // Increase duty cycle up to 100% during INIT state
        //--------------------
        case LED_STATE_INIT:        
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
                g_duty8 = 255;
            }
            else
            {
                // Duty = linear increase
                g_duty8 += LED_INIT_DUTY_INCREMENT;
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
                g_duty8 = 0;
            }
            else
            {
                // Duty = configured value
                g_duty8 = g_ledConfig.activeDuty;
            }
            // Check total time
            if(g_activeTotalTime >= g_ledConfig.activeTimePER)
            {
                // Restart Cycle
                g_activeTotalTime = 0;
                // Duty = configured value
                g_duty8 = g_ledConfig.activeDuty;
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
                if(g_transmitTotalTime >= g_ledConfig.transmitTimeON)
                {
                    // Duty = OFF (after Time ON)
                    g_duty8 = 0;
                }
                else
                {
                    // Duty = configured value
                    g_duty8 = g_ledConfig.transmitDuty;
                }
                // Check total time
                if(g_transmitTotalTime >= g_ledConfig.transmitTimePER)
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
    // Update LED output with Duty cycle
    g_duty16 = (uint16_t)(g_duty8);
    // Set Duty Cycle for LED output
    // TO DO
}

/* Set LED Config */
void led_setLedConfig(volatile ledConfig* config)
{
    // Copy data
    memcpy((void*)&(g_ledConfig),(const void*)(config), sizeof(ledConfig));
}

/* Get LED Config */
void led_getLedConfig(volatile ledConfig* config)
{
    // Copy data
    memcpy((void*)(config),(const void*)&(g_ledConfig), sizeof(ledConfig));
}

/* Request LED Transition signaling */
void led_requestTransmitSignaling(void)
{
    g_transmitRequested = true;    
}
