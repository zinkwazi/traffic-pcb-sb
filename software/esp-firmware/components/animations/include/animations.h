/**
 * animations.h
 * 
 * Contains functions that create iterators to determine proper ordering
 * of LEDs during a refresh to display a particular animation.
 */

#ifndef ANIMATIONS_H_
#define ANIMATIONS_H_

#include <stdint.h>

#include "esp_err.h"

#include "led_coordinates.h"

esp_err_t orderLEDs(int32_t ledOrder[static MAX_NUM_LEDS_COORDS + 1], Animation anim);


#endif /* ANIMATIONS_H_ */