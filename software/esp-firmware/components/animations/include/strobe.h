/**
 * strobe.h
 * 
 * Contains functionality for interacting with the strobe task.
 */

#ifndef STROBE_H_4_5_25
#define STROBE_H_4_5_25

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t strobeRegisterLED(uint16_t ledNum, uint8_t maxBrightness, uint8_t minBrightness, uint8_t initBrightness, bool initStrobeUp);
esp_err_t strobeUnregisterLED(uint16_t ledNum);
esp_err_t strobeUnregisterAll(void);

#endif /* STROBE_H_4_5_25 */