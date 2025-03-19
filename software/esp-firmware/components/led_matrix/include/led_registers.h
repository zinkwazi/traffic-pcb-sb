/**
 * led_registers.h
 */

#ifndef LED_REGISTERS_H_
#define LED_REGISTERS_H_

#include "sdkconfig.h"

#include "led_types.h"

#if CONFIG_HARDWARE_VERSION == 1
/** @brief The maximum number of LEDs that can be present on the device.
*         This should be equal to MAX_NUM_LEDS_COORD.
*/
#define MAX_NUM_LEDS_REG 326
#elif CONFIG_HARDWARE_VERSION == 2
/** @brief The maximum number of LEDs that can be present on the device.
*         This should be equal to MAX_NUM_LEDS_COORD.
*/
#define MAX_NUM_LEDS_REG 414
#else
#error "Unsupported Hardware Version"
#endif

extern const LEDReg LEDNumToReg[MAX_NUM_LEDS_REG];

#endif /* LED_REGISTERS_H_ */