/**
 * led_matrix.h
 */

#ifndef d_MATRIX_H_
#define d_MATRIX_H_

#include <stdint.h>

#include "driver/i2c_types.h"
#include "driver/gpio.h"
#include "esp_err.h"

enum PWMFrequency {
    TWENTY_NINE_K = 0,
    MATRIX_PWMFREQ_INVALID_1 = 1,
    THREE_POINT_SIX_K = 2,
    MATRIX_PWMFREQ_INVALID_3 = 3,
    MATRIX_PWMFREQ_INVALID_4 = 4,
    MATRIX_PWMFREQ_INVALID_5 = 5,
    MATRIX_PWMFREQ_INVALID_6 = 6,
    ONE_POINT_EIGHT_K = 7,
    MATRIX_PWMFREQ_INVALID_8 = 8,
    MATRIX_PWMFREQ_INVALID_9 = 9,
    MATRIX_PWMFREQ_INVALID_10 = 10,
    NINE_HUNDRED = 11,
    MATRIX_PWMFREQ_MAX = 12, // indicates start of invalid values
};

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

esp_err_t matSetOperatingMode(enum Operation setting);
esp_err_t matSetOpenShortDetection(enum ShortDetectionEnable setting);
esp_err_t matSetLogicLevel(enum LogicLevel setting);
esp_err_t matSetSWxSetting(enum SWXSetting setting);
esp_err_t matSetGlobalCurrentControl(uint8_t value);
esp_err_t matSetResistorPullupSetting(enum ResistorSetting setting);
esp_err_t matSetResistorPulldownSetting(enum ResistorSetting setting);
esp_err_t matSetPWMFrequency(enum PWMFrequency freq);
esp_err_t matReset(void);
esp_err_t matSetColor(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue);
esp_err_t matGetColor(uint16_t ledNum, uint8_t *red, uint8_t *green, uint8_t *blue);
esp_err_t matSetScaling(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue);
esp_err_t matGetScaling(uint16_t ledNum, uint8_t *red, uint8_t *green, uint8_t *blue);


#if CONFIG_HARDWARE_VERSION == 1

esp_err_t matInitialize(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin);

#elif CONFIG_HARDWARE_VERSION == 2

esp_err_t matInitializeBus1(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin);
esp_err_t matInitializeBus2(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin);

#else
#error "Unsupported hardware version!"
#endif


#if CONFIG_DISABLE_TESTING_FEATURES == false

esp_err_t matReleaseBus(void);

#endif /* CONFIG_DISABLE_TESTING_FEATURES == false */

#endif /* d_MATRIX_H_ */