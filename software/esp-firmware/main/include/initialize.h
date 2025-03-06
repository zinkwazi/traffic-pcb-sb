/**
 * initialize.h
 * 
 * Contains functions that initialize various hardware and software components.
 */

#ifndef INITIALIZE_H_
#define INITIALIZE_H_

#include <stdint.h>

#include "esp_err.h"
#include "sdkconfig.h"

#include "main_types.h"

esp_err_t initializeApplication(MainTaskState *state, MainTaskResources *res);
esp_err_t initializeMatrices(void);
esp_err_t initializeIndicatorLEDs(void);

#if CONFIG_HARDWARE_VERSION == 1

/* no version specific functions */

#elif CONFIG_HARDWARE_VERSION == 2

esp_err_t initLEDLegendHeavy(uint8_t red, uint8_t green, uint8_t blue);
esp_err_t initLEDLegendMedium(uint8_t red, uint8_t green, uint8_t blue);
esp_err_t initLEDLegendLight(uint8_t red, uint8_t green, uint8_t blue);

#else
#error "Unsupported hardware version!"
#endif

#endif /* INITIALIZE_H_ */