/**
 * d_matrix.h
 */

#ifndef d_MATRIX_H_
#define d_MATRIX_H_

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

enum Operation {
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

struct PageState {
    uint8_t mat1;
    uint8_t mat2;
    uint8_t mat3;
};

// TODO: replace static variables with thread local storage
static struct PageState currState;
static QueueHandle_t I2CQueue; // uninitialized until gatekeeper task is created
// TODO: Check whether these are NULL on 
//       reset or only on new flash.
static i2c_master_bus_handle_t master_bus;
static i2c_master_dev_handle_t matrix1_handle;
static i2c_master_dev_handle_t matrix2_handle;
static i2c_master_dev_handle_t matrix3_handle;

static inline void dotsResetStaticVars(void) {
    currState.mat1 = 0;
    currState.mat2 = 0;
    currState.mat3 = 0;
    I2CQueue = NULL;
    master_bus = NULL;
    matrix1_handle = NULL;
    matrix2_handle = NULL;
    matrix3_handle = NULL;
}

/* Functions available as gatekeeper commands */
esp_err_t dSetOperatingMode(enum Operation setting);
esp_err_t dSetOpenShortDetection(enum ShortDetectionEnable setting);
esp_err_t dSetLogicLevel(enum LogicLevel setting);
esp_err_t dSetSWxSetting(enum SWXSetting setting);
esp_err_t dSetGlobalCurrentControl(uint8_t value);
esp_err_t dSetResistorPullupSetting(enum ResistorSetting setting);
esp_err_t dSetResistorPulldownSetting(enum ResistorSetting setting);
esp_err_t dSetPWMFrequency(enum PWMFrequency freq);
esp_err_t dReset(void);
esp_err_t dSetColor(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue);
esp_err_t dSetScaling(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue);

/* Internal functions */
esp_err_t dInitializeBus(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin);
esp_err_t dAssertConnected(void);
void dSetBits(uint8_t *reg, uint8_t bitMask, uint8_t value);
esp_err_t dSetPage(i2c_master_dev_handle_t device, uint8_t page);
esp_err_t dGetRegister(uint8_t *result, i2c_master_dev_handle_t device, uint8_t page, uint8_t addr);
esp_err_t dGetRegisters(uint8_t *result1, uint8_t *result2, uint8_t *result3, uint8_t page, uint8_t addr);
esp_err_t dSetRegister(i2c_master_dev_handle_t device, uint8_t page, uint8_t addr, uint8_t data);
esp_err_t dSetRegisters(uint8_t page, uint8_t addr, uint8_t data);
esp_err_t dSetRegistersSeparate(uint8_t page, uint8_t addr, uint8_t mat1val, uint8_t mat2val, uint8_t mat3val);

#endif /* d_MATRIX_H_ */