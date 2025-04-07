/**
 * indicators.h
 * 
 * Contains functions for indicating non-error events.
 */

#ifndef INDICATORS_H_
#define INDICATORS_H_

#include <stdint.h>

#include "esp_err.h"

#include "app_errors.h"

#include "main_types.h"

esp_err_t indicateWifiConnected(void);
esp_err_t indicateWifiNotConnected(void);
esp_err_t indicateOTAAvailable(void);
esp_err_t indicateOTAUpdate(void);
esp_err_t indicateOTAFailure(ErrorResources *errRes, int32_t delay);
esp_err_t indicateOTASuccess(int32_t delay);
esp_err_t indicateNorthbound(void);
esp_err_t indicateSouthbound(void);
esp_err_t indicateDirection(Direction dir);

#if CONFIG_HARDWARE_VERSION == 1

/* no hardware specific functions */

#elif CONFIG_HARDWARE_VERSION == 2

esp_err_t indicateOTAAvailable(void);

#else
#error "Unsupported hardware version!"
#endif

#endif /* INDICATORS_H_ */