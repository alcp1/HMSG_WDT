#ifndef LED_H
#define LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

//----------------------------------------------------------------------------//
// EXTERNAL DEFINITIONS
//----------------------------------------------------------------------------//
typedef enum
{
    LED_TRANS_SIG_I2C = 0,
    LED_TRANS_SIG_HMSG
} led_trans_t;

//----------------------------------------------------------------------------//
// EXTERNAL TYPES
//----------------------------------------------------------------------------//
// LED Config frame
typedef struct  
{
    uint16_t activeDuty;
    uint16_t activeTimeON;
    uint16_t activeTimePER;
    uint16_t transmitI2CDuty;
    uint16_t transmitI2CTimeON;
    uint16_t transmitI2CTimePER;
    uint16_t transmitHMSGDuty;
    uint16_t transmitHMSGTimeON;
    uint16_t transmitHMSGTimePER;
} ledConfig;

//----------------------------------------------------------------------------//
// EXTERNAL FUNCTIONS
//----------------------------------------------------------------------------//
/**
 * LED Initialization.
 * 
 * \param   nothing
 * \return  nothing
 */
extern void led_init(void);

/**
 * Periodic function.
 * 
 * \param   nothing
 * \return  nothing
 */
extern void led_periodic(void);

/**
 * Set LED Config.
 * 
 * \param   config LED configuration
 * \return  nothing
 */
extern void led_setLedConfig(ledConfig* config);

/**
 * Get LED Config.
 * 
 * \param   config LED configuration
 * \return  nothing
 */
extern void led_getLedConfig(ledConfig* config);

/**
 * Request LED Transition signaling.
 * 
 * \param   nothing
 * \return  nothing
 */
extern void led_requestTransmitSignaling(led_trans_t type);

#ifdef __cplusplus
}
#endif

#endif /* LED_H */

