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
STATIC_IF_NOT_TEST esp_err_t handleActionQueryOTA(void);

#endif /* ACTIONS_PI_H_4_19_25 */