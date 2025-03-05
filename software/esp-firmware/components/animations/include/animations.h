/**
 * animations.h
 * 
 * Contains functionality for determining the proper ordering of LEDs in order
 * to animate board refreshes in some way.
 */

#ifndef ANIMATIONS_H_
#define ANIMATIONS_H_

#include <stdint.h>

#include "esp_err.h"

#include "animation_types.h"

enum Animation {
    DIAG_LINE = 0,
    DIAG_LINE_REVERSE = 1,
    CURVED_LINE = 2,
    CURVED_LINE_REVERSE = 3,
    ANIM_MAXIMUM = 4,
};

typedef enum Animation Animation;

esp_err_t orderLEDs(int32_t ledOrder[], int32_t ledOrderLen, Animation anim, const LEDCoord coords[], int32_t coordsLen);

#endif /* ANIMATIONS_H_ */