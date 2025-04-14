/**
 * strobe.h
 * 
 * Contains functionality for interacting with the strobe task.
 */

#ifndef STROBE_H_4_5_25
#define STROBE_H_4_5_25

#include "sdkconfig.h"

#ifdef CONFIG_SUPPORT_STROBING

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"

#include "esp_err.h"

/* Holds command information for the strobe task, passed via the strobe command queue */
struct StrobeTaskCommand {
    /* the calling task, necessary to synchronize unregistering */
    TaskHandle_t caller; // ignored. This is populated by the API.
    /* the target LED hardware number. An LED number of UINT16_MAX indicates unregister all. */
    uint16_t ledNum;
    /* the initial brightness direction. Ignored if register LED is false. */
    uint8_t initScale;
    /* The initial strobing direction. Ignored if register LED is false. */
    bool initStrobeUp;
    /* the maximum brightness value. Ignored if registerLED is false. */
    uint8_t maxScale;
    /* the minimum brightness value. Ignored if registerLED is false. */
    uint8_t minScale;
    /* the brightness step size when the brightness is above stepCutoff.
    This controls the speed of strobing. If a faster speed is necessary, but 
    quantization is an issue, consider reducing the strobe task period. */
    uint8_t stepSizeHigh;
    /* the brightness step size when the brightness is below stepCutoff. */
    uint8_t stepSizeLow;
    /* the brightness cutoff at which the step size switches from stepSizeHigh
    to stepSizeLow. */
    uint8_t stepCutoff;
    /* a callback to perform once minScale is reached. The callback may call
    any API functions below, such as unregistering the LED or modifying
    parameters of the strobe state, which will take effect during the next
    strobe period. Even modifying which callback is performed! Must be NULL
    if unused. Must be a best-effort function, ie. don't kill the strobe task
    by delaying forever. Long callbacks may cause the strobe task to miss
    a deadline. */
    // void (*doneCallback)(StrobeLED *info);
    /* whether to register or unregister strobing on the LED */
    bool registerLED; // ignored. This is populated by the API.
};

typedef struct StrobeTaskCommand StrobeTaskCommand;

int getStrobeTaskPeriod(void);
esp_err_t pauseStrobeRegisterLEDs(TickType_t blockTime);
esp_err_t resumeStrobeRegisterLEDs(void);
esp_err_t strobeRegisterLED(StrobeTaskCommand strobeCommand);
esp_err_t strobeUnregisterLED(uint16_t ledNum);
esp_err_t strobeUnregisterAll(void);

#endif /* CONFIG_SUPPORT_STROBING */
#endif /* STROBE_H_4_5_25 */