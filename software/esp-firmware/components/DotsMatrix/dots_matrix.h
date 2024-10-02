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

enum PWMFrequency {
    TWENTY_NINE_K = 0,
    THREE_POINT_SIX_K = 2,
    ONE_POINT_EIGHT_K = 7,
    NINE_HUNDRED = 11,
};

enum ResistorSetting {
    NONE = 0,
    HALF_K = 1,
    ONE_K = 2,
    TWO_K = 3,
    FOUR_K = 4,
    EIGHT_K = 5,
    SIXTEEN_K = 6,
    THIRTY_TWO_K = 7,
};

enum SoftwareShutdown {
    SOFTWARE_SHUTDOWN = 0,
    NORMAL_OPERATION = 1,
};

enum ShortDetectionEnable {
    DISABLE_DETECTION = 0,
    OPEN_DETECTION = 1,
    SHORT_DETECTION = 2,
    REDUNDANT_OPEN_DETECTION = 3,
};

enum LogicLevel {
    STANDARD = 0,
    ALTERNATE = 1,
};

enum SWXSetting {
    NINE = 0,
    EIGHT = 1,
    SEVEN = 2,
    SIX = 3,
    FIVE = 4,
    FOUR = 5,
    THREE = 6,
    TWO = 7,
    CURRENT_SINK_ONLY = 8,
};

/**
 * This enum defines all possible commands
 * to be given to the I2C gatekeeper. An
 * enum is used instead of a function pointer
 * to prevent a malicious task from breaking
 * the gatekeeper in some way. Each entry in
 * the enum corresponds to a function below.
 */
enum I2CCommandFunc {
    INITIALIZE_BUS, // dotsInitializeBus
    ASSERT_CONNECTED, // dotsAssertConnected
};

typedef enum I2CCommandFunc I2CCommandFunc;

struct I2CCommand {
    I2CCommandFunc func; // command to be given to the gatekeeper (ie. function)
    void *params; // parameters to be given to the function, see respective enum below
    void (*errCallback)(esp_err_t err); // callback which will be called upon an error
    // TODO: figure out a more secure way to perform error handling
};

typedef struct I2CCommand I2CCommand;

struct I2CGatekeeperTaskParameters {
    QueueHandle_t I2CQueueHandle;
};

typedef struct I2CGatekeeperTaskParameters I2CGatekeeperTaskParameters;

void vI2CGatekeeperTask(void *pvParameters);
void executeI2CCommand(I2CCommand *command);

esp_err_t dotsInitializeBus(void);
esp_err_t dotsAssertConnected(void);
esp_err_t dotsChangePWMFrequency(enum PWMFrequency freq);
esp_err_t dotsChangeResistorSetting(enum ResistorSetting setting);
esp_err_t dotsSoftwareShutdown(void);
esp_err_t dotsNormalOperation(void);
esp_err_t dotsShortDetectionEnable(void);

#endif /* DOTS_MATRIX_H_ */