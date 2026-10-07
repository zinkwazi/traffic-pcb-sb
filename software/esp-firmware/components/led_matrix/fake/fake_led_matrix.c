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
#include "freertos/FreeRTOS.h"

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

/**
 * An injected fault on a single function of led_matrix.h.
 */
typedef struct {
    esp_err_t err;               // the error to return when failing
    uint32_t successesRemaining; // calls to let succeed before failing
    uint32_t failuresRemaining;  // calls to fail; the fault is inactive at 0
    uint32_t callCount;          // calls made since the last fakeMatResetFaults
} FakeMatFault;

static bool ledMatrixComponentInitialized = false;
static FakeLEDMatrix fakeLEDMatrix;
static FakeMatFault fakeMatFaults[FAKE_MAT_FUNCTION_MAX];
static portMUX_TYPE fakeMatFaultsLock = portMUX_INITIALIZER_UNLOCKED;

static bool matrixIsValid(Matrix matrix);
static void resetFakeLEDMatrix(void);
static bool consumeFault(FakeMatFunction func, esp_err_t *err);

/**
 * Returns the injected error from the calling function if a
 * fault is due on it. A failing call has no side effects.
 */
#define RETURN_IF_FAULT(func)                       \
    do {                                            \
        esp_err_t injectedErr;                      \
        if (consumeFault((func), &injectedErr))     \
        {                                           \
            return injectedErr;                     \
        }                                           \
    } while (0)

/**
 * Injects a fault into a function of the fake. The function first
 * succeeds numSuccessesBefore times, then returns err without side
 * effects for the next numFailures calls, then behaves normally again.
 * Injecting a fault into a function replaces any pending fault on it.
 *
 * @param func The function to inject the fault into.
 * @param err The error the function returns while failing.
 * @param numSuccessesBefore The number of calls to let succeed first.
 * @param numFailures The number of calls to fail.
 *
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if func is invalid, err is ESP_OK, or numFailures is 0.
 */
esp_err_t fakeMatInjectFault(FakeMatFunction func, esp_err_t err, uint32_t numSuccessesBefore, uint32_t numFailures)
{
    if ((uint32_t) func >= FAKE_MAT_FUNCTION_MAX) return ESP_ERR_INVALID_ARG;
    if (err == ESP_OK) return ESP_ERR_INVALID_ARG;
    if (numFailures == 0) return ESP_ERR_INVALID_ARG;

    taskENTER_CRITICAL(&fakeMatFaultsLock);
    fakeMatFaults[func].err = err;
    fakeMatFaults[func].successesRemaining = numSuccessesBefore;
    fakeMatFaults[func].failuresRemaining = numFailures;
    taskEXIT_CRITICAL(&fakeMatFaultsLock);
    return ESP_OK;
}

/**
 * Clears all pending faults and call counts. This does not
 * change the state of the fake LED matrices.
 */
void fakeMatResetFaults(void)
{
    taskENTER_CRITICAL(&fakeMatFaultsLock);
    for (uint32_t i = 0; i < FAKE_MAT_FUNCTION_MAX; i++)
    {
        fakeMatFaults[i].err = ESP_OK;
        fakeMatFaults[i].successesRemaining = 0;
        fakeMatFaults[i].failuresRemaining = 0;
        fakeMatFaults[i].callCount = 0;
    }
    taskEXIT_CRITICAL(&fakeMatFaultsLock);
}

/**
 * Returns the number of calls made to a function since the last
 * fakeMatResetFaults, including calls that failed due to a fault.
 *
 * @param func The function to get the call count of.
 *
 * @returns The call count, or 0 if func is invalid.
 */
uint32_t fakeMatGetCallCount(FakeMatFunction func)
{
    if ((uint32_t) func >= FAKE_MAT_FUNCTION_MAX) return 0;

    taskENTER_CRITICAL(&fakeMatFaultsLock);
    uint32_t callCount = fakeMatFaults[func].callCount;
    taskEXIT_CRITICAL(&fakeMatFaultsLock);
    return callCount;
}

esp_err_t initLedMatrix(void)
{
    RETURN_IF_FAULT(FAKE_MAT_INIT_LED_MATRIX);
    if (ledMatrixComponentInitialized) THROW_ERR((esp_err_t) ESP_ERR_INVALID_STATE);

    ledMatrixComponentInitialized = true;
    resetFakeLEDMatrix();
    return ESP_OK;
}

esp_err_t getLedMatrixStatus(void)
{
    RETURN_IF_FAULT(FAKE_MAT_GET_LED_MATRIX_STATUS);
    if (!ledMatrixComponentInitialized) return ESP_FAIL;

    return ESP_OK;
}

esp_err_t matSetOperatingMode(enum Operation setting)
{
    RETURN_IF_FAULT(FAKE_MAT_SET_OPERATING_MODE);
    if (setting >= MATRIX_OPERATION_MAX) return ESP_ERR_INVALID_ARG;

    fakeLEDMatrix.operationSetting = setting;
    return ESP_OK;
}

esp_err_t matGetOperatingMode(enum Operation *setting, Matrix matrix)
{
    RETURN_IF_FAULT(FAKE_MAT_GET_OPERATING_MODE);
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
    RETURN_IF_FAULT(FAKE_MAT_SET_OPEN_SHORT_DETECTION);
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
    RETURN_IF_FAULT(FAKE_MAT_GET_OPEN_SHORT_DETECTION);
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
    RETURN_IF_FAULT(FAKE_MAT_SET_LOGIC_LEVEL);
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
    RETURN_IF_FAULT(FAKE_MAT_GET_LOGIC_LEVEL);
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
    RETURN_IF_FAULT(FAKE_MAT_SET_SWX_SETTING);
    if (setting >= MATRIX_SWXSETTING_MAX) return ESP_ERR_INVALID_ARG;

    fakeLEDMatrix.swxSetting = setting;
    return ESP_OK;
}

esp_err_t matGetSWxSetting(enum SWXSetting *setting, Matrix matrix)
{
    RETURN_IF_FAULT(FAKE_MAT_GET_SWX_SETTING);
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
    RETURN_IF_FAULT(FAKE_MAT_SET_GLOBAL_CURRENT_CONTROL);
    fakeLEDMatrix.globalCurrentControl = value;
    return ESP_OK;
}

esp_err_t matGetGlobalCurrentControl(uint8_t *value, Matrix matrix)
{
    RETURN_IF_FAULT(FAKE_MAT_GET_GLOBAL_CURRENT_CONTROL);
    if (NULL == value) return ESP_ERR_INVALID_ARG;
    if (!matrixIsValid(matrix)) return ESP_ERR_INVALID_ARG;

    *value = fakeLEDMatrix.globalCurrentControl;
    return ESP_OK;
}

esp_err_t matSetResistorPullupSetting(enum ResistorSetting setting)
{
    RETURN_IF_FAULT(FAKE_MAT_SET_RESISTOR_PULLUP_SETTING);
    if (setting >= MATRIX_RESISTORSETTING_MAX) return ESP_ERR_INVALID_ARG;

    fakeLEDMatrix.resistorPullupSetting = setting;
    return ESP_OK;
}

esp_err_t matGetResistorPullupSetting(enum ResistorSetting *setting, Matrix matrix)
{
    RETURN_IF_FAULT(FAKE_MAT_GET_RESISTOR_PULLUP_SETTING);
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
    RETURN_IF_FAULT(FAKE_MAT_SET_RESISTOR_PULLDOWN_SETTING);
    if (setting >= MATRIX_RESISTORSETTING_MAX) return ESP_ERR_INVALID_ARG;

    fakeLEDMatrix.resistorPulldownSetting = setting;
    return ESP_OK;
}

esp_err_t matGetResistorPulldownSetting(enum ResistorSetting *setting, Matrix matrix)
{
    RETURN_IF_FAULT(FAKE_MAT_GET_RESISTOR_PULLDOWN_SETTING);
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
    RETURN_IF_FAULT(FAKE_MAT_RESET);
    resetFakeLEDMatrix();
    return ESP_OK;
}

esp_err_t matGetDeviceID(uint8_t *id, Matrix matrix)
{
    RETURN_IF_FAULT(FAKE_MAT_GET_DEVICE_ID);
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
    RETURN_IF_FAULT(FAKE_MAT_SET_COLOR);
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
    RETURN_IF_FAULT(FAKE_MAT_GET_COLOR);
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
    RETURN_IF_FAULT(FAKE_MAT_SET_SCALING);
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
    RETURN_IF_FAULT(FAKE_MAT_GET_SCALING);
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
    RETURN_IF_FAULT(FAKE_MAT_INITIALIZE);
    ESP_UNUSED(port);
    ESP_UNUSED(sdaPin);
    ESP_UNUSED(sclPin);
    return ESP_OK;
}

#elif CONFIG_HARDWARE_VERSION == 2

esp_err_t matSetGCCByAmbientLight(void)
{
    RETURN_IF_FAULT(FAKE_MAT_SET_GCC_BY_AMBIENT_LIGHT);
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

static void resetFakeLEDMatrix(void)
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
}

/**
 * Records a call to func and determines whether it must fail.
 *
 * @param func The function being called.
 * @param[out] err The error to return if the call must fail.
 *
 * @returns True if the call must fail with err, otherwise false.
 */
static bool consumeFault(FakeMatFunction func, esp_err_t *err)
{
    bool fail = false;

    taskENTER_CRITICAL(&fakeMatFaultsLock);
    FakeMatFault *fault = &fakeMatFaults[func];
    fault->callCount++;
    if (fault->failuresRemaining > 0)
    {
        if (fault->successesRemaining > 0)
        {
            fault->successesRemaining--;
        }
        else
        {
            fault->failuresRemaining--;
            *err = fault->err;
            fail = true;
        }
    }
    taskEXIT_CRITICAL(&fakeMatFaultsLock);
    return fail;
}

#endif /* defined(CONFIG_FAKE_LED_MATRIX) */