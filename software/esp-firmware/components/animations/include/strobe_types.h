/**
 * strobe_types.h
 * 
 * Contains strobe types used in strobe.h and strobe_task.h.
 */

#ifndef STROBE_TYPES_H_4_5_25
#define STROBE_TYPES_H_4_5_25

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_errors.h"

/* Holds information necessary for the strobe task to strobe an LED */
struct StrobeLED {
    /* the calling task, necessary to keep track of valid unregister commands */
    TaskHandle_t caller;
    /* the target LED hardware number */
    uint16_t ledNum;
    /* the maximum brightness value */
    uint8_t maxScale;
    /* the minimum brightness value */
    uint8_t minScale;
    /* scaling value */
    uint8_t currScale;
    /* the brightness step size when the brightness is above stepCutoff.
    This controls the speed of strobing. If a faster speed is necessary, but 
    quantization is an issue, consider reducing the strobe task period. */
    uint8_t stepSizeHigh;
    /* the brightness step size when the brightness is below stepCutoff. */
    uint8_t stepSizeLow;
    /* the brightness cutoff at which the step size switches from stepSizeHigh
    to stepSizeLow. */
    uint8_t stepCutoff;
    /* scaling direction */
    bool scalingUp;
};

typedef struct StrobeLED StrobeLED;

#endif /* STROBE_TYPES_H_4_5_25 */