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
    /* scaling value */
    uint8_t currScale;
    /* scaling direction */
    bool scalingUp;
};

typedef struct StrobeLED StrobeLED;

/* Holds command information for the strobe task, passed via the strobe command queue */
struct StrobeTaskCommand {
    /* the calling task, necessary to synchronize unregistering */
    TaskHandle_t caller;
    /* the target LED hardware number. An LED number of UINT16_MAX indicates unregister all. */
    uint16_t ledNum;
    /* whether to register or unregister strobing on the LED */
    bool registerLED;
};

typedef struct StrobeTaskCommand StrobeTaskCommand;

/* Holds resources passed to the strobe task during task creation */
struct StrobeTaskResources {
    /* error resources necessary for the task to throw application errors */
    ErrorResources *errRes;
};

    typedef struct StrobeTaskResources StrobeTaskResources;

#endif /* STROBE_TYPES_H_4_5_25 */