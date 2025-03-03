/**
 * led_coordinates.h
 * 
 * This is a layer of indirection to particular led_coordinate version
 * header files, which includes the correct header for the version of
 * hardware the build is targeting.
 */

#ifndef LED_COORDINATES_H_
#define LED_COORDINATES_H_

#include "sdkconfig.h"

#include "animation_types.h"

#if CONFIG_HARDWARE_VERSION == 1
#define MAX_NUM_LEDS_COORDS 326
#elif CONFIG_HARDWARE_VERSION == 2
#define MAX_NUM_LEDS_COORDS 414
#else
#error "Unsupported Hardware Version"
#endif

extern const LEDCoord LEDNumToCoord[MAX_NUM_LEDS_COORDS];


#endif /* LED_COORDINATES_H_ */