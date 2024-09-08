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

/* Component includes */
#include "pinout.h"

static i2c_master_bus_handle_t master_bus;
static i2c_master_dev_handle_t matrix1_handle;
static i2c_master_dev_handle_t matrix2_handle;
static i2c_master_dev_handle_t matrix3_handle;

esp_err_t dotsInitializeBus(void)
{
    const i2c_master_bus_config_t master_bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT, // not sure about this
        .glitch_ignore_cnt = 7, // typical value
        .intr_priority = 0, // may be one of level 1, 2, or 3 when set to 0
        .flags.enable_internal_pullup = false,
    };

    // i2c_master_bus_handle_t master_bus;
    esp_err_t err = i2c_new_master_bus(&master_bus_config, &master_bus);
    if (err != ESP_OK) {
        return err;
    }

    
    const i2c_device_config_t matrix1_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x00,
        .scl_speed_hz = 400000, // 400kHz maximum
        .scl_wait_us = 0, // use default value
    };

    const i2c_device_config_t matrix2_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x11,
        .scl_speed_hz = 400000, // 400kHz maximum
        .scl_wait_us = 0, // use default value
    };

    const i2c_device_config_t matrix3_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x10,
        .scl_speed_hz = 400000, // 400kHz maximum
        .scl_wait_us = 0, // use default value
    };
    
    err = i2c_master_bus_add_device(master_bus, &matrix1_config, &matrix1_handle);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_master_bus_add_device(master_bus, &matrix2_config, &matrix2_handle);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_master_bus_add_device(master_bus, &matrix3_config, &matrix3_handle);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

esp_err_t dotsAssertConnected(void) {
    return ESP_OK;
}