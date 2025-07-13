/**
 * actions_pi.h
 * 
 * Private interface for actions.c white-box testing.
 */

#ifndef ACTIONS_PI_H_4_19_25
#define ACTIONS_PI_H_4_19_25

#include "esp_err.h"

#include "utilities.h"

STATIC_IF_NOT_TEST esp_err_t handleActionUpdateData(void);

#if CONFIG_HARDWARE_VERSION == 1
/* feature unsupported */
#elif CONFIG_HARDWARE_VERSION == 2
STATIC_IF_NOT_TEST esp_err_t handleActionUpdateBrightness(void);
STATIC_IF_NOT_TEST esp_err_t handleActionQueryOTA(void);
STATIC_IF_NOT_TEST esp_err_t handleActionStartNighttimeMode(void);
STATIC_IF_NOT_TEST esp_err_t handleActionEndNighttimeMode(void);
#else
#error "Unsupported hardware version!"
#endif

#endif /* ACTIONS_PI_H_4_19_25 */