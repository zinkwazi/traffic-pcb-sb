/**
 * animations.h
 * 
 * Contains functions that create iterators to determine proper ordering
 * of LEDs during a refresh to display a particular animation.
 */

#ifndef ANIMATIONS_H_
#define ANIMATIONS_H_

#include "esp_err.h"

#include <stdint.h>

esp_err_t sortLEDsByDistanceFromDiagLine(int32_t ledArr[], int32_t ledArrLen);
esp_err_t sortLEDsByDistParabolicMap(int32_t ledArr[], int32_t ledArrLen);

#endif /* ANIMATIONS_H_ */