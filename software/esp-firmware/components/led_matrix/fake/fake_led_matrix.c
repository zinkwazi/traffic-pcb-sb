/**
 * fake_led_matrix.c
 * 
 * Created On: 9/5/2026
 * Author: Jaden Baptista
 * 
 * Contains a fake implementation of functions in led_matrix.h
 * to allow testing hardware dependencies.
 */
#include "sdkconfig.h"
#if defined(CONFIG_FAKE_LED_MATRIX)

#include "led_matrix.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_types.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_macros.h"

#include "app_err.h"
#include "led_registers.h"
#include "led_types.h"

#define TAG "led_matrix"

#if CONFIG_HARDWARE_VERSION == 1

/* ID register readback = 7-bit I2C slave address << 1 */
#define MATRIX1_DEVICE_ID       (0x60)
#define MATRIX2_DEVICE_ID       (0x66)
#define MATRIX3_DEVICE_ID       (0x64)

#elif CONFIG_HARDWARE_VERSION == 2

/* ID register readback = 7-bit I2C slave address << 1 */
#define MATRIX1_DEVICE_ID       (0x60)
#define MATRIX2_DEVICE_ID       (0x66)
#define MATRIX3_DEVICE_ID       (0x60)
#define MATRIX4_DEVICE_ID       (0x66)

#else
#error "Unsupported hardware version!"
#endif

/**
 * The fake LED matrix state. Settings can only be
 * for every matrix all at once, so this only needs
 * to keep track of a single setting.
 */
typedef struct {
    enum ResistorSetting resistorPullupSetting;
    enum ResistorSetting resistorPulldownSetting;
    enum Operation operationSetting;
    enum ShortDetectionEnable shortDetectionEnableSetting;
    enum LogicLevel logicLevelSetting;
    enum SWXSetting swxSetting;
    uint8_t globalCurrentControl;
    uint8_t ledRed[MAX_NUM_LEDS_REG];
    uint8_t ledGreen[MAX_NUM_LEDS_REG];
    uint8_t ledBlue[MAX_NUM_LEDS_REG];
    uint8_t scalingRed[MAX_NUM_LEDS_REG];
    uint8_t scalingGreen[MAX_NUM_LEDS_REG];
    uint8_t scalingBlue[MAX_NUM_LEDS_REG];
} FakeLEDMatrix;

static bool ledMatrixComponentInitialized = false;
static FakeLEDMatrix fakeLEDMatrix;

static bool matrixIsValid(Matrix matrix);

esp_err_t initLedMatrix(void)
{
    if (ledMatrixComponentInitialized) THROW_ERR((esp_err_t) ESP_ERR_INVALID_STATE);

    ledMatrixComponentInitialized = true;
    return matReset();
}

esp_err_t getLedMatrixStatus(void)
{
    if (!ledMatrixComponentInitialized) return ESP_FAIL;

    return ESP_OK;
}

esp_err_t matSetOperatingMode(enum Operation setting)
{
    if (setting >= MATRIX_OPERATION_MAX) return ESP_ERR_INVALID_ARG;

    fakeLEDMatrix.operationSetting = setting;
    return ESP_OK;
}

esp_err_t matGetOperatingMode(enum Operation *setting, Matrix matrix)
{
    if (NULL == setting) return ESP_ERR_INVALID_ARG;
    if (!matrixIsValid(matrix))
    {
        *setting = MATRIX_OPERATION_MAX;
        return ESP_ERR_INVALID_ARG;
    }

    *setting = fakeLEDMatrix.operationSetting;
    return ESP_OK;
}

esp_err_t matSetOpenShortDetection(enum ShortDetectionEnable setting)
{
    switch (setting)
    {
        case DISABLE_DETECTION:
        case OPEN_DETECTION:
        case SHORT_DETECTION:
        case REDUNDANT_OPEN_DETECTION:
            fakeLEDMatrix.shortDetectionEnableSetting = setting;
            return ESP_OK;
        default:
            break;
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t matGetOpenShortDetection(enum ShortDetectionEnable *setting, Matrix matrix)
{
    if (NULL == setting) return ESP_ERR_INVALID_ARG;
    if (!matrixIsValid(matrix))
    {
        *setting = MATRIX_SHORT_DETECTION_EN_MAX;
        return ESP_ERR_INVALID_ARG;
    }

    *setting = fakeLEDMatrix.shortDetectionEnableSetting;
    return ESP_OK;
}

esp_err_t matSetLogicLevel(enum LogicLevel setting)
{
    switch (setting)
    {
        case STANDARD:
        case ALTERNATE:
            fakeLEDMatrix.logicLevelSetting = setting;
            return ESP_OK;
        default:
            break;
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t matGetLogicLevel(enum LogicLevel *setting, Matrix matrix)
{
    if (NULL == setting) return ESP_ERR_INVALID_ARG;
    if (!matrixIsValid(matrix))
    {
        *setting = MATRIX_LOGIC_LEVEL_MAX;
        return ESP_ERR_INVALID_ARG;
    }

    *setting = fakeLEDMatrix.logicLevelSetting;
    return ESP_OK;
}

esp_err_t matSetSWxSetting(enum SWXSetting setting)
{
    if (setting >= MATRIX_SWXSETTING_MAX) return ESP_ERR_INVALID_ARG;

    fakeLEDMatrix.swxSetting = setting;
    return ESP_OK;
}

esp_err_t matGetSWxSetting(enum SWXSetting *setting, Matrix matrix)
{
    if (NULL == setting) return ESP_ERR_INVALID_ARG;
    if (!matrixIsValid(matrix))
    {
        *setting = MATRIX_SWXSETTING_MAX;
        return ESP_ERR_INVALID_ARG;
    }

    *setting = fakeLEDMatrix.swxSetting;
    return ESP_OK;
}

esp_err_t matSetGlobalCurrentControl(uint8_t value)
{
    fakeLEDMatrix.globalCurrentControl = value;
    return ESP_OK;
}

esp_err_t matGetGlobalCurrentControl(uint8_t *value, Matrix matrix)
{
    if (NULL == value) return ESP_ERR_INVALID_ARG;
    if (!matrixIsValid(matrix)) return ESP_ERR_INVALID_ARG;

    *value = fakeLEDMatrix.globalCurrentControl;
    return ESP_OK;
}

esp_err_t matSetResistorPullupSetting(enum ResistorSetting setting)
{
    if (setting >= MATRIX_RESISTORSETTING_MAX) return ESP_ERR_INVALID_ARG;

    fakeLEDMatrix.resistorPullupSetting = setting;
    return ESP_OK;
}

esp_err_t matGetResistorPullupSetting(enum ResistorSetting *setting, Matrix matrix)
{
    if (NULL == setting) return ESP_ERR_INVALID_ARG;
    if (!matrixIsValid(matrix))
    {
        *setting = MATRIX_RESISTORSETTING_MAX;
        return ESP_ERR_INVALID_ARG;
    }

    *setting = fakeLEDMatrix.resistorPullupSetting;
    return ESP_OK;
}

esp_err_t matSetResistorPulldownSetting(enum ResistorSetting setting)
{
    if (setting >= MATRIX_RESISTORSETTING_MAX) return ESP_ERR_INVALID_ARG;

    fakeLEDMatrix.resistorPulldownSetting = setting;
    return ESP_OK;
}

esp_err_t matGetResistorPulldownSetting(enum ResistorSetting *setting, Matrix matrix)
{
    if (NULL == setting) return ESP_ERR_INVALID_ARG;
    if (!matrixIsValid(matrix))
    {
        *setting = MATRIX_RESISTORSETTING_MAX;
        return ESP_ERR_INVALID_ARG;
    }

    *setting = fakeLEDMatrix.resistorPulldownSetting;
    return ESP_OK;
}

esp_err_t matReset(void)
{
    fakeLEDMatrix.resistorPullupSetting = (enum ResistorSetting) 0;
    fakeLEDMatrix.resistorPulldownSetting = (enum ResistorSetting) 0;
    fakeLEDMatrix.operationSetting = (enum Operation) 0;
    fakeLEDMatrix.shortDetectionEnableSetting = (enum ShortDetectionEnable) 0;
    fakeLEDMatrix.logicLevelSetting = (enum LogicLevel) 0;
    fakeLEDMatrix.swxSetting = (enum SWXSetting) 0;
    fakeLEDMatrix.globalCurrentControl = 0;
    for (uint32_t i = 0; i < MAX_NUM_LEDS_REG; i++)
    {
        fakeLEDMatrix.ledRed[i] = 0;
        fakeLEDMatrix.ledGreen[i] = 0;
        fakeLEDMatrix.ledBlue[i] = 0;
        fakeLEDMatrix.scalingRed[i] = 0;
        fakeLEDMatrix.scalingGreen[i] = 0;
        fakeLEDMatrix.scalingBlue[i] = 0;
    }

    return ESP_OK;
}

esp_err_t matGetDeviceID(uint8_t *id, Matrix matrix)
{
    if (id == NULL) return ESP_ERR_INVALID_ARG;

    switch (matrix)
    {
        case MATRIX1:
            *id = MATRIX1_DEVICE_ID;
            break;
        case MATRIX2:
            *id = MATRIX2_DEVICE_ID;
            break;
        case MATRIX3:
            *id = MATRIX3_DEVICE_ID;
            break;
#if CONFIG_HARDWARE_VERSION == 2
        case MATRIX4:
            *id = MATRIX4_DEVICE_ID;
            break;
#endif
        default:
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t matSetColor(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue)
{
    if (ledNum == 0) return ESP_ERR_INVALID_ARG;
    if (ledNum > MAX_NUM_LEDS_REG) return ESP_ERR_INVALID_ARG;
    if (!isLEDValid(LEDNumToReg[ledNum - 1])) return APP_ERR_INVALID_PAGE;

    fakeLEDMatrix.ledRed[ledNum - 1] = red;
    fakeLEDMatrix.ledGreen[ledNum - 1] = green;
    fakeLEDMatrix.ledBlue[ledNum - 1] = blue;
    return ESP_OK;
}

esp_err_t matGetColor(uint16_t ledNum, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    if (red == NULL || green == NULL || blue == NULL) return ESP_ERR_INVALID_ARG;
    if (ledNum == 0) return ESP_ERR_INVALID_ARG;
    if (ledNum > MAX_NUM_LEDS_REG) return ESP_ERR_INVALID_ARG;
    if (!isLEDValid(LEDNumToReg[ledNum - 1])) return APP_ERR_INVALID_PAGE;

    *red = fakeLEDMatrix.ledRed[ledNum - 1];
    *green = fakeLEDMatrix.ledGreen[ledNum - 1];
    *blue = fakeLEDMatrix.ledBlue[ledNum - 1];
    return ESP_OK;
}

esp_err_t matSetScaling(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue)
{
    if (ledNum == 0) return ESP_ERR_INVALID_ARG;
    if (ledNum > MAX_NUM_LEDS_REG) return ESP_ERR_INVALID_ARG;
    if (!isLEDValid(LEDNumToReg[ledNum - 1])) return APP_ERR_INVALID_PAGE;

    fakeLEDMatrix.scalingRed[ledNum - 1] = red;
    fakeLEDMatrix.scalingGreen[ledNum - 1] = green;
    fakeLEDMatrix.scalingBlue[ledNum - 1] = blue;
    return ESP_OK;
}

esp_err_t matGetScaling(uint16_t ledNum, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    if (red == NULL || green == NULL || blue == NULL) return ESP_ERR_INVALID_ARG;
    if (ledNum == 0) return ESP_ERR_INVALID_ARG;
    if (ledNum > MAX_NUM_LEDS_REG) return ESP_ERR_INVALID_ARG;
    if (!isLEDValid(LEDNumToReg[ledNum - 1])) return APP_ERR_INVALID_PAGE;

    *red = fakeLEDMatrix.scalingRed[ledNum - 1];
    *green = fakeLEDMatrix.scalingGreen[ledNum - 1];
    *blue = fakeLEDMatrix.scalingBlue[ledNum - 1];
    return ESP_OK;
}

#if CONFIG_HARDWARE_VERSION == 1

esp_err_t matInitialize(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin)
{
    ESP_UNUSED(port);
    ESP_UNUSED(sdaPin);
    ESP_UNUSED(sclPin);
    return ESP_OK;
}

#elif CONFIG_HARDWARE_VERSION == 2

esp_err_t matSetGCCByAmbientLight(void)
{
    fakeLEDMatrix.globalCurrentControl = CONFIG_GLOBAL_LED_CURRENT;
    return ESP_OK;
}

#else
#error "Unsupported hardware version!"
#endif

static bool matrixIsValid(Matrix matrix)
{
#if CONFIG_HARDWARE_VERSION == 1
    return matrix == MATRIX1 || matrix == MATRIX2 || matrix == MATRIX3;
#elif CONFIG_HARDWARE_VERSION == 2
    return matrix == MATRIX1 || matrix == MATRIX2 || matrix == MATRIX3 || matrix == MATRIX4;
#endif
}

#endif /* defined(CONFIG_FAKE_LED_MATRIX) */