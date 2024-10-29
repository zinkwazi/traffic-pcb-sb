/**
 * dots_commands.c
 * 
 * This file contains wrapper functions that 
 * place an element on the I2C gatekeeper queue 
 * to interact with LED matrices.
 */

#include "dots_commands.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2c_master.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_check.h"

#include "dots_matrix.h"

/* A struct for placing parameters on the I2C command queue */
struct SetColorParams {
    uint16_t ledNum;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

/* A struct for placing parameters on the I2C command queue */
struct SetScalingParams {
    uint16_t ledNum;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

/**
 * This function maps the I2CCommandFunc enum to actual functions
 * and executes them, performing error callbacks when necessary.
 */
void executeI2CCommand(I2CCommand *command) {
    esp_err_t err = ESP_OK;
    ESP_LOGD(TAG, "executing I2C command...");
    switch (command->func) {
        case SET_OPERATING_MODE:
            ESP_LOGD(TAG, "setting operating mode");
            err = dSetOperatingMode(*((enum Operation*) command->params));
            break;
        case SET_OPEN_SHORT_DETECTION:
            ESP_LOGD(TAG, "changing open/short detection");
            err = dSetOpenShortDetection(*((enum ShortDetectionEnable*) command->params));
            break;
        case SET_LOGIC_LEVEL:
            ESP_LOGD(TAG, "changing logic level");
            err = dSetLogicLevel(*((enum LogicLevel*) command->params));
            break;
        case SET_SWX_SETTING:
            ESP_LOGD(TAG, "changing SWx setting");
            err = dSetSWxSetting(*((enum SWXSetting*) command->params));
            break;
        case SET_GLOBAL_CURRENT_CONTROL:
            ESP_LOGD(TAG, "changing global current control setting");
            err = dSetGlobalCurrentControl(*((uint8_t*) command->params));
            break;
        case SET_RESISTOR_PULLUP:
            ESP_LOGD(TAG, "changing resistor pullup setting");
            err = dSetResistorPullupSetting(*((enum ResistorSetting*) command->params));
            break;
        case SET_RESISTOR_PULLDOWN:
            ESP_LOGD(TAG, "changing resistor pulldown setting");
            err = dSetResistorPulldownSetting(*((enum ResistorSetting*) command->params));
            break;
        case SET_PWM_FREQUENCY:
            ESP_LOGD(TAG, "changing PWM frequency");
            err = dSetPWMFrequency(*((enum PWMFrequency*) command->params));
            break;
        case RESET:
            ESP_LOGD(TAG, "resetting matrices");
            err = dReset();
            break;
        case SET_COLOR:
            ESP_LOGD(TAG, "changing dot color");
            struct SetColorParams *setColorParams = (struct SetColorParams*) command->params;
            err = dSetColor(setColorParams->ledNum, 
                            setColorParams->red, 
                            setColorParams->green, 
                            setColorParams->blue);
            break;
        case SET_SCALING:
            ESP_LOGD(TAG, "changing dot scaling");
            struct SetScalingParams *setScalingParams = (struct SetScalingParams*) command->params;
            err = dSetScaling(setScalingParams->ledNum,
                              setScalingParams->red, 
                              setScalingParams->green, 
                              setScalingParams->blue);
            break;
        default:
            ESP_LOGW(TAG, "I2C gatekeeper recieved an invalid command");
            return;
    }
    if (err != ESP_OK) {
        if (command->errCallback != NULL) {
            command->errCallback(err); // TODO: replace this with a more secure error handling method
        } else {
            ESP_LOGE(TAG, "I2C gatekeeper recieved a null error callback function!");
        }
    }
}

/**
 * This task manages interaction with the I2C peripheral,
 * which should be interacted with only through the functions
 * below. These functions abstract queueing interaction with 
 * the d matrices.
 */
void vI2CGatekeeperTask(void *pvParameters) {
    struct I2CGatekeeperTaskParameters *params = (struct I2CGatekeeperTaskParameters *) pvParameters;
    I2CCommand command;
    esp_err_t err;
    /* One time setup */
    dotsResetStaticVars();
    err = dInitializeBus();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize I2C bus");
    }
    err = ESP_FAIL;
    while (err != ESP_OK) {
        err = dAssertConnected();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "could not find i2c matrices... retrying...");
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
    /* Wait for commands and execute them forever */
    for (;;) {  // This task should never end
        if (pdPASS != xQueueReceive(params->I2CQueueHandle, &command, INT_MAX)) {
            ESP_LOGD(TAG, "I2C Gatekeeper timed out while waiting for command on queue");
            continue;
        }
        executeI2CCommand(&command);
        if (command.params != NULL) {
            free(command.params);
            command.params = NULL;
        }
    }
    ESP_LOGE(TAG, "I2C gatekeeper task is exiting! This should be impossible!");
    vTaskDelete(NULL); // exit safely (should never happen)
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * into the provided operation mode.
 * 
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dotsSetOperatingMode(QueueHandle_t queue, enum Operation setting) {
    /* copy params to heap */
    enum Operation *heapSetting = malloc(sizeof(enum Operation));
    *heapSetting = setting;
    /* add command to I2C queue */
    I2CCommand command = { // queueing is by copy, not reference
        .func = SET_OPERATING_MODE,
        .params = heapSetting,
        .errCallback = NULL
    };
    if (xQueueSendToBack(queue, &command, 0) != pdTRUE) {
        ESP_LOGE(TAG, "failed to add command to queue");
        return ESP_FAIL;
    };
    return ESP_OK;
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * into the provided detection mode.
 * 
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dotsSetOpenShortDetection(QueueHandle_t queue, enum ShortDetectionEnable setting) {
    /* copy params to heap */
    enum ShortDetectionEnable *heapSetting = malloc(sizeof(enum ShortDetectionEnable));
    *heapSetting = setting;
    /* add command to I2C queue */
    I2CCommand command = { // queueing is by copy, not reference
        .func = SET_OPEN_SHORT_DETECTION,
        .params = heapSetting,
        .errCallback = NULL
    };
    if (xQueueSendToBack(queue, &command, 0) != pdTRUE) {
        ESP_LOGE(TAG, "failed to add command to queue");
        return ESP_FAIL;
    };
    return ESP_OK;
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * into the provided logic level.
 * 
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dotsSetLogicLevel(QueueHandle_t queue, enum LogicLevel setting) {
    /* copy params to heap */
    enum LogicLevel *heapSetting = malloc(sizeof(enum LogicLevel));
    *heapSetting = setting;
    /* add command to I2C queue */
    I2CCommand command = { // queueing is by copy, not reference
        .func = SET_LOGIC_LEVEL,
        .params = heapSetting,
        .errCallback = NULL
    };
    if (xQueueSendToBack(queue, &command, 0) != pdTRUE) {
        ESP_LOGE(TAG, "failed to add command to queue");
        return ESP_FAIL;
    };
    return ESP_OK;
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * int othe provided SWx setting.
 * 
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dotsSetSWxSetting(QueueHandle_t queue, enum SWXSetting setting) {
    /* copy params to heap */
    enum SWXSetting *heapSetting = malloc(sizeof(enum SWXSetting));
    *heapSetting = setting;
    /* add command to I2C queue */
    I2CCommand command = { // queueing is by copy, not reference
        .func = SET_SWX_SETTING,
        .params = heapSetting,
        .errCallback = NULL
    };
    if (xQueueSendToBack(queue, &command, 0) != pdTRUE) {
        ESP_LOGE(TAG, "failed to add command to queue");
        return ESP_FAIL;
    };
    return ESP_OK;
}

/**
 * Performs I2C transactions to change the global current
 * control setting of each matrix to the provided value.
 * 
 * Returns: ESP_OK if successful. Otherwise, the register value
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dotsSetGlobalCurrentControl(QueueHandle_t queue, uint8_t value) {
    /* copy params to heap */
    uint8_t *heapValue = malloc(sizeof(uint8_t));
    *heapValue = value;
    /* add command to I2C queue */
    I2CCommand command = { // queueing is by copy, not reference
        .func = SET_GLOBAL_CURRENT_CONTROL,
        .params = heapValue,
        .errCallback = NULL
    };
    if (xQueueSendToBack(queue, &command, 0) != pdTRUE) {
        ESP_LOGE(TAG, "failed to add command to queue");
        return ESP_FAIL;
    };
    return ESP_OK;
}

/**
 * Performs I2C transactions to change the resistor pullup
 * value of each matrix to the provided value.
 * 
 * Returns: ESP_OK if successful. Otherwise, the register value
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dotsSetResistorPullupSetting(QueueHandle_t queue, enum ResistorSetting setting) {
    /* copy params to heap */
    enum ResistorSetting *heapSetting = malloc(sizeof(enum ResistorSetting));
    *heapSetting = setting;
    /* add command to I2C queue */
    I2CCommand command = { // queueing is by copy, not reference
        .func = SET_RESISTOR_PULLUP,
        .params = heapSetting,
        .errCallback = NULL
    };
    if (xQueueSendToBack(queue, &command, 0) != pdTRUE) {
        ESP_LOGE(TAG, "failed to add command to queue");
        return ESP_FAIL;
    };
    return ESP_OK;
}

/**
 * Performs I2C transactions to change the resistor pulldown
 * value of each matrix to the provided value.
 * 
 * Returns: ESP_OK if successful. Otherwise, the register value
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dotsSetResistorPulldownSetting(QueueHandle_t queue, enum ResistorSetting setting) {
    /* copy params to heap */
    enum ResistorSetting *heapSetting = malloc(sizeof(enum ResistorSetting));
    *heapSetting = setting;
    /* add command to I2C queue */
    I2CCommand command = { // queueing is by copy, not reference
        .func = SET_RESISTOR_PULLDOWN,
        .params = heapSetting,
        .errCallback = NULL
    };
    if (xQueueSendToBack(queue, &command, 0) != pdTRUE) {
        ESP_LOGE(TAG, "failed to add command to queue");
        return ESP_FAIL;
    };
    return ESP_OK;
}

/** 
 * Sets the PWM frequency of all matrix ICs. 
 */
esp_err_t dotsSetPWMFrequency(QueueHandle_t queue, enum PWMFrequency freq) {
    /* copy params to heap */
    enum PWMFrequency *heapFreq = malloc(sizeof(enum PWMFrequency));
    *heapFreq = freq;
    /* add command to I2C queue */
    I2CCommand command = { // queueing is by copy, not reference
        .func = SET_PWM_FREQUENCY,
        .params = heapFreq,
        .errCallback = NULL
    };
    if (xQueueSendToBack(queue, &command, 0) != pdTRUE) {
        ESP_LOGE(TAG, "failed to add command to queue");
        return ESP_FAIL;
    };
    return ESP_OK;
}

/**
 * Resets all matrix registers to default values.
 * 
 * Returns: ESP_OK if successful. Otherwise, some of
 *          the matrices may have been reset, but not all.
 */
esp_err_t dotsReset(QueueHandle_t queue) {
    /* add command to I2C queue */
    I2CCommand command = { // queueing is by copy, not reference
        .func = RESET,
        .params = NULL,
        .errCallback = NULL
    };
    if (xQueueSendToBack(queue, &command, 0) != pdTRUE) {
        ESP_LOGE(TAG, "failed to add command to queue");
        return ESP_FAIL;
    };
    return ESP_OK;
}


/**
 * Sets the color of the LED corresponding to Kicad hardware
 * number ledNum. Internally, this changes the PWM duty in
 * 256 steps.
 */
esp_err_t dotsSetColor(QueueHandle_t queue, uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue) {
    /* copy parameters to heap */
    struct SetColorParams *heapParams = malloc(sizeof(struct SetColorParams));
    heapParams->ledNum = ledNum;
    heapParams->red = red;
    heapParams->green = green;
    heapParams->blue = blue;
    /* add command to I2C queue */
    I2CCommand command = { // queueing is by copy, not reference
        .func = SET_COLOR,
        .params = heapParams,
        .errCallback = NULL
    };
    while (xQueueSendToBack(queue, &command, INT_MAX) != pdTRUE) {
        ESP_LOGE(TAG, "failed to add command to queue... retrying...");
    }
    return ESP_OK;
}



/**
 * Controls the DC output current of the LED corresponding
 * to Kicad hardware number ledNum. See pg. 13 of the datasheet
 * for exact calculations. This can be considered a dimming
 * function.
 */
esp_err_t dotsSetScaling(QueueHandle_t queue, uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue) {
    /* copy parameters to heap */
    struct SetScalingParams *heapParams = malloc(sizeof(struct SetColorParams));
    heapParams->ledNum = ledNum;
    heapParams->red = red;
    heapParams->green = green;
    heapParams->blue = blue;
    /* add command to I2C queue */
    I2CCommand command = { // queueing is by copy, not reference
        .func = SET_SCALING,
        .params = heapParams,
        .errCallback = NULL
    };
    while (xQueueSendToBack(queue, &command, INT_MAX) != pdTRUE) {
        ESP_LOGE(TAG, "failed to add command to queue... retrying...");
    }
    return ESP_OK;
}