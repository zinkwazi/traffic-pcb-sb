/**
 * led_matrix.h
 * 
 * This file contains a hardware abstraction layer for interaction with 
 * the LED matrix driver ICs through I2C.
 * 
 * See: https://www.lumissil.com/assets/pdf/core/IS31FL3741A_DS.pdf.
 */

#ifndef LED_MATRIX_H_4_8_25
#define LED_MATRIX_H_4_8_25

#include <stdint.h>

#include "sdkconfig.h"
#include "driver/i2c_types.h"
#include "driver/gpio.h"
#include "esp_err.h"

#include "app_err.h"

enum ResistorSetting {
    RES_NONE = 0,
    HALF_K = 1,
    ONE_K = 2,
    TWO_K = 3,
    FOUR_K = 4,
    EIGHT_K = 5,
    SIXTEEN_K = 6,
    THIRTY_TWO_K = 7,
    MATRIX_RESISTORSETTING_MAX = 8, // indicates start of invalid values
};

enum Operation {
    SOFTWARE_SHUTDOWN = 0,
    NORMAL_OPERATION = 1,
    MATRIX_OPERATION_MAX = 2, // indicates start of invalid values
};

enum ShortDetectionEnable {
    DISABLE_DETECTION = 0,
    OPEN_DETECTION = 1,
    SHORT_DETECTION = 2,
    REDUNDANT_OPEN_DETECTION = 3,
    MATRIX_SHORT_DETECTION_EN_MAX = 4, // indicates start of invalid values
};

enum LogicLevel {
    STANDARD = 0,
    ALTERNATE = 1,
    MATRIX_LOGIC_LEVEL_MAX = 2, // indicates start of invalid values
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
    MATRIX_SWXSETTING_MAX = 9, // indicates start of invalid values
};

typedef enum Matrix {
    MATRIX1,
    MATRIX2,
    MATRIX3,
#if CONFIG_HARDWARE_VERISON == 1
    /* none */
#elif CONFIG_HARDWARE_VERSION == 2
    MATRIX4,
#endif
} Matrix;

esp_err_t initLedMatrix(void);
esp_err_t getLedMatrixStatus(void);
esp_err_t matSetOperatingMode(enum Operation setting);
esp_err_t matGetOperatingMode(enum Operation *setting, Matrix matrix);
esp_err_t matSetOpenShortDetection(enum ShortDetectionEnable setting);
esp_err_t matGetOpenShortDetection(enum ShortDetectionEnable *setting, Matrix matrix);
esp_err_t matSetLogicLevel(enum LogicLevel setting);
esp_err_t matGetLogicLevel(enum LogicLevel *setting, Matrix matrix);
esp_err_t matSetSWxSetting(enum SWXSetting setting);
esp_err_t matGetSWxSetting(enum SWXSetting *setting, Matrix matrix);
esp_err_t matSetGlobalCurrentControl(uint8_t value);
esp_err_t matGetGlobalCurrentControl(uint8_t *value, Matrix matrix);
esp_err_t matSetResistorPullupSetting(enum ResistorSetting setting);
esp_err_t matGetResistorPullupSetting(enum ResistorSetting *setting, Matrix matrix);
esp_err_t matSetResistorPulldownSetting(enum ResistorSetting setting);
esp_err_t matGetResistorPulldownSetting(enum ResistorSetting *setting, Matrix matrix);
/* PWM frequency select (36h) removed: it never worked on the hardware this
was tested against (LCSC-sourced IS31FL3741A-marked chips, two batches). */
esp_err_t matReset(void);
esp_err_t matGetDeviceID(uint8_t *id, Matrix matrix);
esp_err_t matSetColor(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue);
esp_err_t matGetColor(uint16_t ledNum, uint8_t *red, uint8_t *green, uint8_t *blue);
esp_err_t matSetScaling(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue);
esp_err_t matGetScaling(uint16_t ledNum, uint8_t *red, uint8_t *green, uint8_t *blue);

#if CONFIG_HARDWARE_VERSION == 1
esp_err_t matInitialize(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin);
#elif CONFIG_HARDWARE_VERSION == 2
esp_err_t matSetGCCByAmbientLight(void);
#else
#error "Unsupported hardware version!"
#endif

#if defined(CONFIG_FAKE_LED_MATRIX)

/**
 * Identifies a function of this header for fault injection
 * into the led_matrix fake.
 */
typedef enum FakeMatFunction {
    FAKE_MAT_INIT_LED_MATRIX,
    FAKE_MAT_GET_LED_MATRIX_STATUS,
    FAKE_MAT_SET_OPERATING_MODE,
    FAKE_MAT_GET_OPERATING_MODE,
    FAKE_MAT_SET_OPEN_SHORT_DETECTION,
    FAKE_MAT_GET_OPEN_SHORT_DETECTION,
    FAKE_MAT_SET_LOGIC_LEVEL,
    FAKE_MAT_GET_LOGIC_LEVEL,
    FAKE_MAT_SET_SWX_SETTING,
    FAKE_MAT_GET_SWX_SETTING,
    FAKE_MAT_SET_GLOBAL_CURRENT_CONTROL,
    FAKE_MAT_GET_GLOBAL_CURRENT_CONTROL,
    FAKE_MAT_SET_RESISTOR_PULLUP_SETTING,
    FAKE_MAT_GET_RESISTOR_PULLUP_SETTING,
    FAKE_MAT_SET_RESISTOR_PULLDOWN_SETTING,
    FAKE_MAT_GET_RESISTOR_PULLDOWN_SETTING,
    FAKE_MAT_RESET,
    FAKE_MAT_GET_DEVICE_ID,
    FAKE_MAT_SET_COLOR,
    FAKE_MAT_GET_COLOR,
    FAKE_MAT_SET_SCALING,
    FAKE_MAT_GET_SCALING,
#if CONFIG_HARDWARE_VERSION == 1
    FAKE_MAT_INITIALIZE,
#elif CONFIG_HARDWARE_VERSION == 2
    FAKE_MAT_SET_GCC_BY_AMBIENT_LIGHT,
#endif
    FAKE_MAT_FUNCTION_MAX, // indicates start of invalid values
} FakeMatFunction;

esp_err_t fakeMatInjectFault(FakeMatFunction func, esp_err_t err, uint32_t numSuccessesBefore, uint32_t numFailures);
void fakeMatResetFaults(void);
uint32_t fakeMatGetCallCount(FakeMatFunction func);

#endif /* defined(CONFIG_FAKE_LED_MATRIX) */

#endif /* LED_MATRIX_H_4_8_25 */