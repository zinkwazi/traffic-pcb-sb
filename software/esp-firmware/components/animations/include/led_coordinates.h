/**
 * led_coordinates.h
 * 
 * This is a layer of indirection to particular led_coordinate version
 * header files, which includes the correct header for the version of
 * hardware the build is targeting.
 */

#ifndef LED_COORDINATES_H_
#define LED_COORDINATES_H_

#if CONFIG_HARDWARE_VERSION == 1
#include "V1_0_led_coordinates.h"
#endif

#if CONFIG_HARDWARE_VERSION == 2
#include "V2_0_led_coordinates.h"
#endif

/** @brief The maximum number of LEDs that can be present on the device.
 *         This should be equal to MAX_NUM_LEDS_REG.
 */
#define MAX_NUM_LEDS_COORDS sizeof(LEDNumToCoord) / sizeof(LEDNumToCoord[0])

#endif /* LED_COORDINATES_H_ */