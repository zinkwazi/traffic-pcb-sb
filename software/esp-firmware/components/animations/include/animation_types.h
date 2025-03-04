/**
 * animation_types.h
 * 
 * Contains types necessary for animation logic.
 */

#ifndef ANIMATION_TYPES_H_
#define ANIMATION_TYPES_H_

#include <stdint.h>

enum Animation {
    DIAG_LINE,
    DIAG_LINE_REVERSE,
    CURVED_LINE,
    CURVED_LINE_REVERSE,
};

typedef enum Animation Animation;

struct LEDCoord {
    int32_t x;
    int32_t y;
};

typedef struct LEDCoord LEDCoord;

#endif /* ANIMATION_TYPES_H_ */