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
#include "esp_check.h"

/* Component includes */
#include "pinout.h"

#define TAG "DotsMatrix"

#define MAT_UPPER_ADDR  0b01100000
#define MAT1_LOWER_ADDR 0x00
#define MAT2_LOWER_ADDR 0x11
#define MAT3_LOWER_ADDR 0x10
#define MAT1_ADDR (MAT_UPPER_ADDR | MAT1_LOWER_ADDR)
#define MAT2_ADDR (MAT_UPPER_ADDR | MAT2_LOWER_ADDR)
#define MAT3_ADDR (MAT_UPPER_ADDR | MAT3_LOWER_ADDR)

#define BUS_SPEED_HZ  400000 // 400kHz maximum
#define SCL_WAIT_US   0  // use default value
#define PROBE_WAIT_MS 100

/* Matrix Driver IC High Level Registers */
#define CMD_REG_ADDR                0xFD
#define CMD_REG_WRITE_LOCK_ADDR     0xFE
#define CMD_REG_WRITE_KEY           0b11000101
#define INTR_MSK_REG_ADDR           0xF0
#define INTR_STAT_REG_ADDR          0xF1
#define ID_REG_ADDR                 0xFC

/* Matrix Driver IC Function Registers */
#define CONFIG_REG_ADDR             0x00
#define CURRENT_CNTRL_REG_ADDR      0x01
#define PULL_SEL_REG_ADDR           0x02
#define PWM_FREQ_REG_ADDR           0x36
#define RESET_REG_ADDR              0x3F

/* Configuration Register Bits */
#define SOFTWARE_SHUTDOWN_BITS      0x01
#define OPEN_SHORT_DETECT_EN_BITS   0x06
#define LOGIC_LEVEL_CNTRL_BITS      0x08
#define SWX_SETTING_BITS            0xF0

/* Pull Up/Down Register Bits */
#define PUR_BITS        0x07
#define PDR_BITS        0x70

/* PWM Frequency Setting Register Bits */
#define PWM_BITS        0x0F

/* Reset Register */
#define RESET_KEY       0xAE


static QueueHandle_t I2CQueue = NULL; // uninitialized until gatekeeper task is created
// TODO: Check whether these are NULL on 
//       reset or only on new flash.
static i2c_master_bus_handle_t master_bus = NULL;
static i2c_master_dev_handle_t matrix1_handle = NULL;
static i2c_master_dev_handle_t matrix2_handle = NULL;
static i2c_master_dev_handle_t matrix3_handle = NULL;

/**
 * This task manages interaction with the I2C peripheral,
 * which should be interacted with only through the functions
 * below. These functions abstract queueing interaction with 
 * the dots matrices.
 */
void vI2CGatekeeperTask(void *pvParameters) {
    struct I2CGatekeeperTaskParameters *params = (struct I2CGatekeeperTaskParameters *) pvParameters;
    I2CCommand command;
    esp_err_t err;
    for (;;) {  // This task should never end
        if (pdPASS != xQueueReceive(params->I2CQueueHandle, &command, INT_MAX)) {
            ESP_LOGD(TAG, "I2C Gatekeeper timed out while waiting for command on queue");
            continue;
        }
        executeI2CCommand(&command);
    }
    ESP_LOGE(TAG, "I2C gatekeeper task is exiting! This should be impossible!");
    vTaskDelete(NULL); // exit safely (should never happen)
}

/**
 * This function maps the I2CCommandFunc enum to actual functions
 * and executes them, performing error callbacks when necessary.
 */
void executeI2CCommand(I2CCommand *command) {
    esp_err_t err = ESP_OK;
    switch (command->func) {
        case INITIALIZE_BUS:
            err = dotsInitializeBus();
            break;
        case ASSERT_CONNECTED:
            err = dotsAssertConnected();
            break;
        default:
            ESP_LOGW(TAG, "I2C gatekeeper recieved an invalid command");
            return;
    }
    if (err != ESP_OK) {
        if (command->errCallback != NULL) {
            command->errCallback(err);
        } else {
            ESP_LOGE(TAG, "I2C gatekeeper recieved a null error callback function!");
        }
    }
}

esp_err_t dotsInitializeBus(void)
{
    i2c_master_bus_config_t master_bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT, // not sure about this
        .glitch_ignore_cnt = 7, // typical value
        .intr_priority = 0, // may be one of level 1, 2, or 3 when set to 0
        .flags.enable_internal_pullup = false,
    };
    i2c_device_config_t matrix_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MAT1_ADDR,
        .scl_speed_hz = BUS_SPEED_HZ,
        .scl_wait_us = SCL_WAIT_US, // use default value
    };
    /* Initialize I2C bus */
    ESP_RETURN_ON_ERROR(
        i2c_new_master_bus(&master_bus_config, &master_bus),
        TAG, "failed to initialize new i2c master bus struct"
    );
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(master_bus, &matrix_config, &matrix1_handle),
        TAG, "failed to add matrix1 device to i2c bus"
    );
    matrix_config.device_address = MAT2_ADDR;
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(master_bus, &matrix_config, &matrix2_handle),
        TAG, "failed to add matrix2 device to i2c bus"
    );
    matrix_config.device_address = MAT3_ADDR;
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(master_bus, &matrix_config, &matrix3_handle),
        TAG, "failed to add matrix3 device to i2c bus"
    );
    return ESP_OK;
}

esp_err_t dotsAssertConnected(void) {
    ESP_RETURN_ON_ERROR(
        i2c_master_probe(master_bus, MAT1_ADDR, PROBE_WAIT_MS),
        TAG, "failed to detect matrix1 on i2c bus"
    );
    ESP_RETURN_ON_ERROR(
        i2c_master_probe(master_bus, MAT2_ADDR, PROBE_WAIT_MS),
        TAG, "failed to detect matrix2 on i2c bus"
    );
    ESP_RETURN_ON_ERROR(
        i2c_master_probe(master_bus, MAT2_ADDR, PROBE_WAIT_MS),
        TAG, "failed to detect matrix2 on i2c bus"
    );
    return ESP_OK;
}

esp_err_t dotsChangePWMFrequency(enum PWMFrequency freq) {

}

esp_err_t dotsChangeResistorSetting(enum ResistorSetting setting) {

}

esp_err_t dotsSoftwareShutdown(void) {

}

esp_err_t dotsNormalOperation(void) {

}

esp_err_t dotsShortDetectionEnable(void) {

}