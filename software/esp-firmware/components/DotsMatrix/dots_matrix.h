/**
 * dots_matrix.h
 */

#ifndef DOTS_MATRIX_H_
#define DOTS_MATRIX_H_

/* IDF component includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2c_master.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"

esp_err_t dotsInitializeBus(void);
esp_err_t dotsAssertConnected(void);

#endif /* DOTS_MATRIX_H_ */