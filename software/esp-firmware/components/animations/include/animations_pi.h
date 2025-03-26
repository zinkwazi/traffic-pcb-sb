/**
 * animations_pi.h
 * 
 * Contains the private interface of animations.c for white-box testing.
 */

#ifndef ANIMATIONS_PI_H_
#define ANIMATIONS_PI_H_
#ifndef CONFIG_DISABLE_TESTING_FEATURES

/* Include the public interface */
#include "animations.h"

#include <stdint.h>

#include "esp_err.h"

#include "animation_types.h"

/*******************************************/
/*            INTERNAL FUNCTIONS           */
/*******************************************/

esp_err_t orderLEDs(int32_t ledOrder[], int32_t ledOrderLen, Animation anim, const LEDCoord coords[], int32_t coordsLen);
double signedDistanceFromDiagLine(LEDCoord coords, double angle);
double signedDistanceFromCurvedLine(LEDCoord coords, double angle);
int compDistFromCurvedLineNorth(const void *c1, const void *c2);
int compDistFromCurvedLineSouth(const void *c1, const void *c2);
esp_err_t sortLEDsByDistanceFromDiagLine(int32_t ledArr[], int32_t ledArrLen, const LEDCoord coords[], int32_t coordsLen);
esp_err_t sortLEDsByDistanceFromCurvedLineNorth(int32_t ledArr[], int32_t ledArrLen, const LEDCoord coords[], int32_t coordsLen);
esp_err_t sortLEDsByDistanceFromCurvedLineSouth(int32_t ledArr[], int32_t ledArrLen, const LEDCoord coords[], int32_t coordsLen);

#else /* ifndef CONFIG_DISABLE_TESTING_FEATURES */
#error "White-box testing header file should not be included in standard builds!"
#endif /* ifndef CONFIG_DISABLE_TESTING_FEATURES */
#endif /* ifndef ANIMATIONS_PI_H_ */