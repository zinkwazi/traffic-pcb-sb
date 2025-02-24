/**
 * dots_commands.h
 */

#ifndef DOTS_COMMANDS_H_
#define DOTS_COMMANDS_H_

/* IDF component includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2c_master.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"

#include "dots_matrix.h"

#define TAG "dots_commands"

#define DOTS_OK_VAL (0x81)
#define DOTS_ERR_VAL (0x6A)

#define DOTS_NOTIFY (true)
#define DOTS_SILENT (false)

#define DOTS_BLOCKING (true)
#define DOTS_ASYNC (false)

/**
 * This enum defines all possible commands
 * that can be given to the I2C gatekeeper. 
 * An enum is used instead of a function 
 * pointer to prevent a malicious task from
 * breaking the gatekeeper in some way. Each 
 * entry in the enum corresponds to a function 
 * below.
 */
enum I2CCommandFunc {
    SET_OPERATING_MODE, // dSetOperatingMode
    SET_OPEN_SHORT_DETECTION, // dSetOpenShortDetection
    SET_LOGIC_LEVEL, // dSetLogicLevel
    SET_SWX_SETTING, // dSetSWxSetting
    SET_GLOBAL_CURRENT_CONTROL, // dSetGlobalCurrentControl
    SET_RESISTOR_PULLUP, // dSetResistorPullupSetting
    SET_RESISTOR_PULLDOWN, // dSetResistorPulldownSetting
    SET_PWM_FREQUENCY, // dSetPWMFrequency
    RESET, // dReset
    SET_COLOR, // dSetColor
    SET_SCALING, // dSetScaling
#if CONFIG_DISABLE_TESTING_FEATURES == false
    RELEASE_BUS,
    REAQUIRE_BUS,
    NOTIFY_OK_VAL,
    NOTIFY_ERR_VAL,
#endif /* CONFIG_DISABLE_TESTING_FEATURES == false */
    I2CCOMMANDFUNC_MAX, // indicates start of invalid functions
};

typedef enum I2CCommandFunc I2CCommandFunc;

struct I2CCommand {
    I2CCommandFunc func; // command to be given to the gatekeeper (ie. function)
    void *params; // parameters to be given to the function, see respective enum below
    TaskHandle_t notifyTask; // The task which will be notified if an error occurs
};

typedef struct I2CCommand I2CCommand;

struct I2CGatekeeperTaskParams {
    QueueHandle_t I2CQueue;
};

typedef struct I2CGatekeeperTaskParams I2CGatekeeperTaskParams;

esp_err_t createI2CGatekeeperTask(TaskHandle_t *handle, QueueHandle_t I2CQueue);
void vI2CGatekeeperTask(void *pvParameters);

esp_err_t dotsSetOperatingMode(QueueHandle_t queue, enum Operation setting, bool notify, bool blocking);
esp_err_t dotsSetOpenShortDetection(QueueHandle_t queue, enum ShortDetectionEnable setting, bool notify, bool blocking);
esp_err_t dotsSetLogicLevel(QueueHandle_t queue, enum LogicLevel setting, bool notify, bool blocking);
esp_err_t dotsSetSWxSetting(QueueHandle_t queue, enum SWXSetting setting, bool notify, bool blocking);
esp_err_t dotsSetGlobalCurrentControl(QueueHandle_t queue, uint8_t value, bool notify, bool blocking);
esp_err_t dotsSetResistorPullupSetting(QueueHandle_t queue, enum ResistorSetting setting, bool notify, bool blocking);
esp_err_t dotsSetResistorPulldownSetting(QueueHandle_t queue, enum ResistorSetting setting, bool notify, bool blocking);
esp_err_t dotsSetPWMFrequency(QueueHandle_t queue, enum PWMFrequency freq, bool notify, bool blocking);
esp_err_t dotsReset(QueueHandle_t queue, bool notify, bool blocking);
esp_err_t dotsSetColor(QueueHandle_t queue, uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue, bool notify, bool blocking);
esp_err_t dotsSetScaling(QueueHandle_t queue, uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue, bool notify, bool blocking);

#endif /* DOTS_COMMANDS_H_ */