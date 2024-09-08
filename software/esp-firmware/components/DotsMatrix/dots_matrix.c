/**
 * dots_matrix.c
 * 
 * This file contains a hardware abstraction layer
 * for the dots matrix led driver ICs. The esp32
 * interacts with these ICs through I2C.
 * 
 * See: https://www.lumissil.com/assets/pdf/core/IS31FL3741A_DS.pdf.
 */

#include "dots_matrix.h"

/* IDF component includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2c_master.h"
#include "esp_system.h"
#include "esp_log.h"


esp_err_t dotsInitializeBus(void) {
    return ESP_OK;
}

esp_err_t dotsAssertConnected(void) {
    return ESP_OK;
}