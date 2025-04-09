/**
 * V2_0_led_matrix.c
 * 
 * This file contains a hardware abstraction layer for interaction with 
 * the LED matrix driver ICs through the I2C bus on V1_0.
 *
 * See: https://www.lumissil.com/assets/pdf/core/IS31FL3741A_DS.pdf.
 */

#include "sdkconfig.h"

#if CONFIG_HARDWARE_VERSION == 1

#include "led_matrix.h"

#include <stdint.h>

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_debug_helpers.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "led_registers.h"
#include "led_types.h"
#include "mat_err.h"

#define TAG "led_matrix"

#define I2C_TIMEOUT_MS 100

#define MAT1_ADDR 0b0110000
#define MAT2_ADDR 0b0110011
#define MAT3_ADDR 0b0110000
#define MAT4_ADDR 0b0110011

#define BUS_SPEED_HZ 400000 // 400kHz maximum
#define SCL_WAIT_US 0       // use default value
#define PROBE_WAIT_MS 10000

/* Matrix Driver IC High Level Registers */
#define CMD_REG_ADDR 0xFD
#define CMD_REG_WRITE_LOCK_ADDR 0xFE
#define CMD_REG_WRITE_KEY 0b11000101
#define INTR_MSK_REG_ADDR 0xF0
#define INTR_STAT_REG_ADDR 0xF1
#define ID_REG_ADDR 0xFC

/* Matrix Driver IC Pages */
#define PWM0_PAGE 0
#define PWM1_PAGE 1
#define SCALING0_PAGE 2
#define SCALING1_PAGE 3
#define CONFIG_PAGE 4

/* Matrix Driver IC Function Registers */
#define CONFIG_REG_ADDR 0x00
#define CURRENT_CNTRL_REG_ADDR 0x01
#define PULL_SEL_REG_ADDR 0x02
#define PWM_FREQ_REG_ADDR 0x36
#define RESET_REG_ADDR 0x3F

/* Configuration Register Bits */
#define SOFTWARE_SHUTDOWN_BITS 0x01
#define OPEN_SHORT_DETECT_EN_BITS 0x06
#define LOGIC_LEVEL_CNTRL_BITS 0x08
#define SWX_SETTING_BITS 0xF0

/* Pull Up/Down Register Bits */
#define PUR_BITS 0x07
#define PDR_BITS 0x70

/* PWM Frequency Setting Register Bits */
#define PWS_BITS 0x0F

/* Reset Register */
#define RESET_KEY 0xAE

#define FILL_BUFFER(reg, data) \
    buffer[0] = reg;           \
    buffer[1] = data;

static i2c_master_bus_handle_t sI2CBus;
static i2c_master_dev_handle_t sMat1Handle;
static i2c_master_dev_handle_t sMat2Handle;
static i2c_master_dev_handle_t sMat3Handle;

static uint8_t sMat1State;
static uint8_t sMat2State;
static uint8_t sMat3State;

/* These mutexes are required in order for the matrix state static variables
above to be stable through multithreading. These mutexes must be obtained before
reading and writing to the state, and must be held throughout the entirety
of a read then write function, otherwise the state may desynchronize from
the actual state of the matrices. The I2C buses themselves are already guarded
by mutexes internally. */
static SemaphoreHandle_t mat1Mutex;
static SemaphoreHandle_t mat2Mutex;
static SemaphoreHandle_t mat3Mutex;

static void matSetBits(uint8_t *reg, uint8_t bitMask, uint8_t value);
static mat_err_t matParseLEDRegisterInfo(i2c_master_dev_handle_t *matrixHandle, uint8_t *pwmPage, uint8_t *scalingPage, LEDReg ledReg);
static mat_err_t matSetPage(i2c_master_dev_handle_t device, uint8_t page);
static mat_err_t matGetRegister(uint8_t *result, i2c_master_dev_handle_t device, uint8_t page, uint8_t addr);
static mat_err_t matGetRegisters(uint8_t *result1, uint8_t *result2, uint8_t *result3, uint8_t page, uint8_t addr);
static mat_err_t matSetRegister(i2c_master_dev_handle_t device, uint8_t page, uint8_t addr, uint8_t data);
static mat_err_t matSetRegisters(uint8_t page, uint8_t addr, uint8_t data);
static mat_err_t matSetRegistersSeparate(uint8_t page, uint8_t addr, uint8_t mat1val, uint8_t mat2val, uint8_t mat3val);
static mat_err_t matSetConfig(uint8_t bitMask, uint8_t setting);

static mat_err_t takeMatrixMutex(i2c_master_dev_handle_t device);
static mat_err_t giveMatrixMutex(i2c_master_dev_handle_t device);

static mat_err_t handleMatSetPageErr(mat_err_t mat_err, i2c_master_dev_handle_t device);

/**
 * @brief Initializes the I2C bus, asserts that the matrices are connected, and 
 * syncs internal state variables to the state of the matrices.
 * 
 * @param[in] port The GPIO port of the first I2C bus.
 * @param[in] sdaPin The GPIO pin of the SDA line.
 * @param[in] sclPin The GPIO pin of the SCL line.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_NOT_FOUND if a matrix on the I2C bus could not be found.
 * MAT_ERR_MUTEX_TIMEOUT if taking a matrix mutex timed out.
 * MAT_ERR_MUTEX if releasing a matrix mutex failed.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matInitialize(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin)
{
    mat_err_t mat_err;
    i2c_master_bus_config_t master_bus_config = {
        .i2c_port = port,
        .sda_io_num = sdaPin,
        .scl_io_num = sclPin,
        .clk_source = I2C_CLK_SRC_DEFAULT, // not sure about this
        .glitch_ignore_cnt = 7,            // typical value
        .intr_priority = 0,                // may be one of level 1, 2, or 3 when set to 0
        .flags.enable_internal_pullup = false,
    };
    i2c_device_config_t matrix_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MAT1_ADDR,
        .scl_speed_hz = BUS_SPEED_HZ,
        .scl_wait_us = SCL_WAIT_US, // use default value
    };

    /* Initialize I2C bus 1 */
    mat_err = (mat_err_t) i2c_new_master_bus(&master_bus_config, &sI2CBus);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    mat_err = (mat_err_t) i2c_master_bus_add_device(sI2CBus, &matrix_config, &sMat1Handle);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    matrix_config.device_address = MAT2_ADDR;
    mat_err = (mat_err_t) i2c_master_bus_add_device(sI2CBus, &matrix_config, &sMat2Handle);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    matrix_config.device_address = MAT3_ADDR;
    mat_err = (mat_err_t) i2c_master_bus_add_device(sI2CBus, &matrix_config, &sMat3Handle);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    /* initialize matrix 1 and 2 mutexes */
    mat1Mutex = xSemaphoreCreateMutex();
    if (mat1Mutex == NULL) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    mat2Mutex = xSemaphoreCreateMutex();
    if (mat2Mutex == NULL) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    mat3Mutex = xSemaphoreCreateMutex();
    if (mat3Mutex == NULL) THROW_MAT_ERR((mat_err_t) ESP_FAIL);

    /* initialize matrix state and sync with matrices. This doubles as a check
    that the matrices are connected. */
    sMat1State = PWM0_PAGE; // force setPage to actually set the page
    sMat2State = PWM0_PAGE; // so that pages are synced with hardware
    sMat3State = PWM0_PAGE;
    mat_err = matSetPage(sMat1Handle, CONFIG_PAGE);
    if (mat_err != ESP_OK) mat_err = handleMatSetPageErr(mat_err, sMat1Handle);
    if (mat_err == ESP_ERR_INVALID_ARG) return (mat_err_t) ESP_FAIL;
    if (mat_err == ESP_ERR_INVALID_STATE) return (mat_err_t) ESP_FAIL;
    if (mat_err == ESP_ERR_TIMEOUT) return (mat_err_t) ESP_ERR_NOT_FOUND;
    if (mat_err == ESP_ERR_INVALID_RESPONSE) return (mat_err_t) ESP_ERR_NOT_FOUND;
    if (mat_err != ESP_OK) return mat_err;
    mat_err = giveMatrixMutex(sMat1Handle); // handleMatSetPageErr does not release if ESP_OK
    if (mat_err != ESP_OK) return MAT_ERR_MUTEX;

    mat_err = matSetPage(sMat2Handle, CONFIG_PAGE);
    if (mat_err != ESP_OK) mat_err = handleMatSetPageErr(mat_err, sMat2Handle);
    if (mat_err == ESP_ERR_INVALID_ARG) return (mat_err_t) ESP_FAIL;
    if (mat_err == ESP_ERR_INVALID_STATE) return (mat_err_t) ESP_FAIL;
    if (mat_err == ESP_ERR_TIMEOUT) return (mat_err_t) ESP_ERR_NOT_FOUND;
    if (mat_err == ESP_ERR_INVALID_RESPONSE) return (mat_err_t) ESP_ERR_NOT_FOUND;
    if (mat_err != ESP_OK) return mat_err;
    mat_err = giveMatrixMutex(sMat2Handle); // handleMatSetPageErr does not release if ESP_OK
    if (mat_err != ESP_OK) return MAT_ERR_MUTEX;

    mat_err = matSetPage(sMat3Handle, CONFIG_PAGE);
    if (mat_err != ESP_OK) mat_err = handleMatSetPageErr(mat_err, sMat3Handle);
    if (mat_err == ESP_ERR_INVALID_ARG) return (mat_err_t) ESP_FAIL;
    if (mat_err == ESP_ERR_INVALID_STATE) return (mat_err_t) ESP_FAIL;
    if (mat_err == ESP_ERR_TIMEOUT) return (mat_err_t) ESP_ERR_NOT_FOUND;
    if (mat_err == ESP_ERR_INVALID_RESPONSE) return (mat_err_t) ESP_ERR_NOT_FOUND;
    if (mat_err != ESP_OK) return mat_err;
    mat_err = giveMatrixMutex(sMat3Handle); // handleMatSetPageErr does not release if ESP_OK
    if (mat_err != ESP_OK) return MAT_ERR_MUTEX;

    return (mat_err_t) ESP_OK;
}

/**
 * @brief Performs I2C transactions to put each of the matrix ICs into the
 * provided operation mode.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] setting The operation mode.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matSetOperatingMode(enum Operation setting)
{
    if (setting >= MATRIX_OPERATION_MAX) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);
    return matSetConfig(SOFTWARE_SHUTDOWN_BITS, (uint8_t) setting);
}

/**
 * @brief Performs I2C transactions to put each of the matrix ICs into the
 * provided detection mode.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] setting The detection mode.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matSetOpenShortDetection(enum ShortDetectionEnable setting)
{
    if (setting >= MATRIX_SHORT_DETECTION_EN_MAX) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);
    return matSetConfig(OPEN_SHORT_DETECT_EN_BITS, (uint8_t) setting);
}

/**
 * @brief Performs I2C transactions to put each of the matrix ICs into the
 * provided logic level.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] setting The logic level.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matSetLogicLevel(enum LogicLevel setting)
{
    if (setting >= MATRIX_LOGIC_LEVEL_MAX) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);
    return matSetConfig(LOGIC_LEVEL_CNTRL_BITS, (uint8_t) setting);
}

/**
 * @brief Performs I2C transactions to put each of the matrix ICs into the
 * provided SWx setting.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] setting The SWx setting.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matSetSWxSetting(enum SWXSetting setting)
{
    if (setting >= MATRIX_SWXSETTING_MAX) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);
    return matSetConfig(SWX_SETTING_BITS, (uint8_t) setting);
}

/**
 * @brief Performs I2C transactions to change the global current
 * control setting of each matrix to the one provided.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] value The new global current control setting.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matSetGlobalCurrentControl(uint8_t value)
{
    mat_err_t mat_err;

    mat_err = matSetRegisters(CONFIG_PAGE, CURRENT_CNTRL_REG_ADDR, value);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    
    return mat_err;
}

/**
 * @brief Performs I2C transactions to change the resistor pullup value of each
 * matrix to the provided value.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] setting The new resistor pullup setting.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matSetResistorPullupSetting(enum ResistorSetting setting)
{
    if (setting >= MATRIX_RESISTORSETTING_MAX) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);
    return matSetConfig(PUR_BITS, (uint8_t) setting);
}

/**
 * @brief Performs I2C transactions to change the resistor pulldown value of each
 * matrix to the provided value.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] setting The new resistor pulldown setting.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matSetResistorPulldownSetting(enum ResistorSetting setting)
{
    if (setting >= MATRIX_RESISTORSETTING_MAX) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);
    return matSetConfig(PDR_BITS, (uint8_t) setting);
}

/**
 * @brief Sets the PWM frequency of all matrix ICs.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] freq The PWM frequency to set the matrices to.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matSetPWMFrequency(enum PWMFrequency freq)
{
    mat_err_t mat_err;
    /* input guards */
    if (freq >= MATRIX_PWMFREQ_MAX) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);

    /* set registers */
    mat_err = matSetRegisters(CONFIG_PAGE, PWM_FREQ_REG_ADDR, (uint8_t) freq);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    return (mat_err_t) ESP_OK;
}

/**
 * @brief Resets all matrix registers to default values.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matReset(void)
{
    mat_err_t mat_err;

    /* set registers */
    mat_err = matSetRegisters(CONFIG_PAGE, RESET_REG_ADDR, RESET_KEY);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    return (mat_err_t) ESP_OK;
}

/**
 * @brief Sets the color of the LED corresponding to Kicad hardware number
 * ledNum. Internally, this changes the PWM duty in 256 steps.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] ledNum The hardware number of the target LED.
 * @param[in] red The color value to set the red LED to.
 * @param[in] green The color value to set the green LED to.
 * @param[in] blue The color value to set the blue LED to.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matSetColor(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue)
{
    mat_err_t mat_err;
    LEDReg ledReg;
    i2c_master_dev_handle_t matrixHandle;
    uint8_t page;
    /* input guards */
    if (ledNum == 0 || ledNum > MAX_NUM_LEDS_REG) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);

    /* determine the correct PWM registers */
    ledReg = LEDNumToReg[ledNum - 1];
    mat_err = matParseLEDRegisterInfo(&matrixHandle, &page, NULL, ledReg);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    /* set PWM registers to provided values */
    mat_err = matSetRegister(matrixHandle, page, ledReg.red, red);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    mat_err = matSetRegister(matrixHandle, page, ledReg.green, green);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    mat_err = matSetRegister(matrixHandle, page, ledReg.blue, blue);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    return (mat_err_t) ESP_OK;
}

/**
 * @brief Sets the brightness, ie. DC output current, of the LED.
 * 
 * @note See pg. 13 of the datasheet for exact calculations.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] ledNum The hardware number of the target LED.
 * @param[in] red The scaling value to set the red LED to.
 * @param[in] green The scaling value to set the green LED to.
 * @param[in] blue The scaling value to set the blue LED to.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
mat_err_t matSetScaling(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue)
{
    /* guard input */
    mat_err_t mat_err;
    LEDReg ledReg;
    i2c_master_dev_handle_t matrixHandle;
    uint8_t page;
    /* input guards */
    if (ledNum == 0 || ledNum > MAX_NUM_LEDS_REG) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);

    /* determine the correct PWM registers */
    ledReg = LEDNumToReg[ledNum - 1];
    mat_err = matParseLEDRegisterInfo(&matrixHandle, NULL, &page, ledReg);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    /* set PWM registers to provided values */
    mat_err = matSetRegister(matrixHandle, page, ledReg.red, red);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    mat_err = matSetRegister(matrixHandle, page, ledReg.green, green);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    mat_err = matSetRegister(matrixHandle, page, ledReg.blue, blue);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    return (mat_err_t) ESP_OK;
}

/**
 * @brief Converts the matrix information in ledReg to an I2C device handle and
 *        page number.
 *
 * @param[out] matrixHandle The location where this function will place the I2C
 *        device handle that corresponds to the matrix enum in ledReg.
 * @param[out] pwmPage The location where this function will place the register
 *        pwm page that corresponds to the matrix enum in ledReg. The function
 *        will not fail if this is NULL, however will fail if both it and
 *        scalingPage are NULL.
 * @param[out] scalingPage The location where this function will place the
 *        register scaling page that corresponds to the matrix enum in ledReg.
 *        The function will not fail if this is NULL, however will fail if both
 *        it and pwmPage are NULL.
 * @param[in] ledReg The register and matrix information that describes where
 *        an LED's registers exist.
 * @param[in] matrices A collection of I2C device handles used to populate
 *        matrixHandle.
 *
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * MAT_ERR_INVALID_PAGE if ledReg.matrix is invalid.
 * ESP_FAIL if an unexpected error occurred.
 */
static mat_err_t matParseLEDRegisterInfo(i2c_master_dev_handle_t *matrixHandle, uint8_t *pwmPage, uint8_t *scalingPage, LEDReg ledReg)
{
    /* input guards */
    if (matrixHandle == NULL) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);
    if (pwmPage == NULL && scalingPage == NULL) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG); // the function assumes at least one of the pages should be returned
    if (ledReg.matrix >= MAT_NONE) THROW_MAT_ERR(MAT_ERR_INVALID_PAGE);

    /* parse information */
    switch (ledReg.matrix)
    {
    case MAT1_PAGE0:
        *matrixHandle = sMat1Handle;
        if (pwmPage != NULL)
        {
            *pwmPage = PWM0_PAGE;
        }
        if (scalingPage != NULL)
        {
            *scalingPage = SCALING0_PAGE;
        }
        break;
    case MAT1_PAGE1:
        *matrixHandle = sMat1Handle;
        if (pwmPage != NULL)
        {
            *pwmPage = PWM1_PAGE;
        }
        if (scalingPage != NULL)
        {
            *scalingPage = SCALING1_PAGE;
        }
        break;
    case MAT2_PAGE0:
        *matrixHandle = sMat2Handle;
        if (pwmPage != NULL)
        {
            *pwmPage = PWM0_PAGE;
        }
        if (scalingPage != NULL)
        {
            *scalingPage = SCALING0_PAGE;
        }
        break;
    case MAT2_PAGE1:
        *matrixHandle = sMat2Handle;
        if (pwmPage != NULL)
        {
            *pwmPage = PWM1_PAGE;
        }
        if (scalingPage != NULL)
        {
            *scalingPage = SCALING1_PAGE;
        }
        break;
    case MAT3_PAGE0:
        *matrixHandle = sMat3Handle;
        if (pwmPage != NULL)
        {
            *pwmPage = PWM0_PAGE;
        }
        if (scalingPage != NULL)
        {
            *scalingPage = SCALING0_PAGE;
        }
        break;
    case MAT3_PAGE1:
        *matrixHandle = sMat3Handle;
        if (pwmPage != NULL)
        {
            *pwmPage = PWM1_PAGE;
        }
        if (scalingPage != NULL)
        {
            *scalingPage = SCALING1_PAGE;
        }
        break;
    default:
        THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    }
    return (mat_err_t) ESP_OK;
}

/**
 * @brief Sets the bits denoted by bitMask to value in reg. Meant to
 * be used to update configuration bits in matrix registers.
 * 
 * @param[out] reg The register value to be changed.
 * @param[in] bitMask The bits to be changed to value.
 * @param[in] value The value to change bitmask bits in reg to. If this
 * value is greater than what bitMask can contain, this value will silently
 * be shortened.
 */
static void matSetBits(uint8_t *reg, uint8_t bitMask, uint8_t value)
{
    /* Align value to bitMask */
    for (uint8_t currShift = 0; currShift < 8; currShift++)
    {
        if (bitMask % (0x01 << currShift) == 0x00) break;
        value <<= currShift;
    }
    /* Update prev mask bits */
    *reg &= ~bitMask;          // clear previous mask bits
    *reg |= (bitMask & value); // set value to mask bits of reg
}

/**
 * @brief Changes the current matrix device register page by unlocking and
 * writing to the command register via I2C. This function takes the device
 * mutex, so the mutex must be released afterward.
 * 
 * @note If the saved state in the ESP32 indicates that the device is already
 * in the requested page, the function will return ESP_OK without performing
 * any I2C transactions.
 * 
 * @see handleMatSetPageErr: handles conditionally releasing the mutex based
 * on the error code of matSetPage, which satisfies requirement 2.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * - giveMatrixMutex(device) is called after this function and interaction 
 * with the device is complete, unless ESP_ERR_INVALID_ARG, 
 * ESP_ERR_INVALID_STATE, or MAT_ERR_MUTEX_TIMEOUT are returned.
 * 
 * @param[in] device The matrix IC device handle.
 * @param[in] page The target page.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if the function requirements are not met.
 * ESP_ERR_INVALID_RESPONSE if the device did not respond with the provided key.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * MAT_ERR_MUTEX_TIMEOUT if taking the device mutex timed out.
 * ESP_FAIL if an unexpected error occurred.
 */
static mat_err_t matSetPage(i2c_master_dev_handle_t device, uint8_t page)
{
    mat_err_t mat_err;
    uint8_t buffer[2];
    /* input guards */
    if (page > 4) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);
    // device argument validated by takeMatrixMutexs

    /* retrieve device mutex */
    mat_err = takeMatrixMutex(device); // caller must release mutex bc there are many return paths
    if (mat_err == ESP_ERR_TIMEOUT) return MAT_ERR_MUTEX_TIMEOUT; // differentiate from other ESP_ERR_TIMEOUT
    if (mat_err != ESP_OK) return mat_err;

    /* check current page setting */
    mat_err = (mat_err_t) ESP_FAIL;
    if (device == sMat1Handle && page == sMat1State)
    {
        mat_err = (mat_err_t) ESP_OK;
    } else if (device == sMat2Handle && page == sMat2State)
    {
        mat_err = (mat_err_t) ESP_OK;
    } else if (device == sMat3Handle && page == sMat3State)
    {
        mat_err = (mat_err_t) ESP_OK;
    }

    if (mat_err == ESP_OK) return (mat_err_t) ESP_OK; // the device is on the target page already

    /* unlock command register */
    FILL_BUFFER(CMD_REG_WRITE_LOCK_ADDR, CMD_REG_WRITE_KEY);
    mat_err = (mat_err_t) i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) THROW_MAT_ERR(mat_err);

    /* confirm unlocked command register */
    FILL_BUFFER(CMD_REG_WRITE_LOCK_ADDR, 0);
    mat_err = (mat_err_t) i2c_master_transmit_receive(device, &(buffer[0]), 1, &(buffer[1]), 1, I2C_TIMEOUT_MS);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) THROW_MAT_ERR(mat_err);

    if (buffer[1] != CMD_REG_WRITE_KEY) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_RESPONSE);

    /* update page */
    FILL_BUFFER(CMD_REG_ADDR, page);
    mat_err = (mat_err_t) i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) THROW_MAT_ERR(mat_err);

    if (device == sMat1Handle)
    {
        sMat1State = page;
    } else if (device == sMat2Handle)
    {
        sMat2State = page;
    } else if (device == sMat3Handle)
    {
        sMat3State = page;
    } else
    {
        /* The device was previously verified as a matrix handle */
        THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    }

    return (mat_err_t) ESP_OK;
}

/**
 * @brief Performs I2C transactions to move matrix IC page to the desired
 * page and reads the IC response of requesting data at the address.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[out] result The location to store the result of the register read.
 * @param[in] device The I2C handle of the matrix IC.
 * @param[in] page The page the target register exists in.
 * @param[in] addr The address of the target register.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * MAT_ERR_MUTEX if the device mutex could not be released.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex
 * is left in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t matGetRegister(uint8_t *result, i2c_master_dev_handle_t device, uint8_t page, uint8_t addr)
{
    mat_err_t mat_err1, mat_err2;
    /* input guards */
    if (result == NULL) return ESP_ERR_INVALID_ARG;
    // device & page validated by matSetPage

    /* Set page and read config registers */
    mat_err1 = matSetPage(device, page); // acquires device mutex
    if (mat_err1 != ESP_OK) return handleMatSetPageErr(mat_err1, device);

    mat_err1 = (mat_err_t) i2c_master_transmit_receive(device, &addr, 1, result, 1, I2C_TIMEOUT_MS);
    // mat_err1 handled after giving up device mutex
    mat_err2 = giveMatrixMutex(device);
    if (mat_err2 != ESP_OK) return MAT_ERR_MUTEX;
    if (mat_err1 == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);
    if (mat_err1 != ESP_OK) THROW_MAT_ERR(mat_err1);
    return (mat_err_t) ESP_OK;
}

/**
 * @brief Performs I2C transactions to move the matrix IC page to the desired
 * page and writes the provided data to the given register address.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] device The I2C handle of the matrix IC.
 * @param[in] page The page the target register exists in.
 * @param[in] addr The address of the register.
 * @param[in] data The data to write to the register.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
static mat_err_t matSetRegister(i2c_master_dev_handle_t device, uint8_t page, uint8_t addr, uint8_t data)
{
    mat_err_t mat_err1, mat_err2;
    uint8_t buffer[2];
    /* gaurd input */
    // device and page are validated by matSetPage

    /* move to the correct page */
    mat_err1 = matSetPage(device, page); // acquires device mutex
    if (mat_err1 != ESP_OK) return handleMatSetPageErr(mat_err1, device);

    /* transmit message */
    FILL_BUFFER(addr, data);
    mat_err1 = (mat_err_t) i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS);
    // mat_err1 handled after giving up device mutex
    mat_err2 = giveMatrixMutex(device);
    if (mat_err2 == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err2 != ESP_OK) return MAT_ERR_MUTEX;
    if (mat_err1 == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err1 != ESP_OK) return mat_err1;
    return (mat_err_t) ESP_OK;
}

/**
 * @brief Sets the target register for all matrix ICs.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] page The page the target register exists in.
 * @param[in] addr The address of the register.
 * @param[in] data The data to write to the register.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
static mat_err_t matSetRegisters(uint8_t page, uint8_t addr, uint8_t data)
{
    mat_err_t mat_err;
    /* guard input */
    // page is validated in matSetRegister

    /* set registers */
    mat_err = matSetRegister(sMat1Handle, page, addr, data);
    if (mat_err != ESP_OK) return mat_err;
    mat_err = matSetRegister(sMat2Handle, page, addr, data);
    if (mat_err != ESP_OK) return mat_err;
    mat_err = matSetRegister(sMat3Handle, page, addr, data);
    if (mat_err != ESP_OK) return mat_err;

    return (mat_err_t) ESP_OK;
}

/**
 * @brief Retrieves the data at the target register for all matrices.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[out] result1 The location to place the register value in matrix 1.
 * @param[out] result2 The location to place the register value in matrix 2.
 * @param[out] result3 The location to place the register value in matrix 3.
 * @param[out] result4 The location to place the register value in matrix 4.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
static mat_err_t matGetRegisters(uint8_t *result1, uint8_t *result2, uint8_t *result3, uint8_t page, uint8_t addr)
{
    mat_err_t mat_err;
    uint8_t localRes1, localRes2, localRes3; // if something fails, do not modify results
    /* guard input */
    if (page > 4) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);

    /* get registers */
    if (result1 != NULL)
    {
        mat_err = matGetRegister(&localRes1, sMat1Handle, page, addr);
        if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
        if (mat_err != ESP_OK) return mat_err;
    }
    if (result2 != NULL)
    {
        mat_err = matGetRegister(&localRes2, sMat2Handle, page, addr);
        if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
        if (mat_err != ESP_OK) return mat_err;
    }
    if (result3 != NULL)
    {
        mat_err = matGetRegister(&localRes3, sMat3Handle, page, addr);
        if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
        if (mat_err != ESP_OK) return mat_err;
    }
    
    /* output results */
    if (result1 != NULL) *result1 = localRes1;
    if (result2 != NULL) *result2 = localRes2;
    if (result3 != NULL) *result3 = localRes3;
    return (mat_err_t) ESP_OK;
}

/**
 * @brief Performs I2C transactions to set the configuration register of each
 * of the matrix ICs to the provided values.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] page The page of the target register.
 * @param[in] addr The address of the target register.
 * @param[in] mat1val The register value for matrix 1.
 * @param[in] mat2val The register value for matrix 2.
 * @param[in] mat3val The register value for matrix 3.
 * @param[in] mat4val The register value for matrix 4.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
static mat_err_t matSetRegistersSeparate(uint8_t page, uint8_t addr, uint8_t mat1val, uint8_t mat2val, uint8_t mat3val)
{
    mat_err_t mat_err;
    /* guard input */
    // page is validated in matSetRegister

    /* set registers */
    mat_err = matSetRegister(sMat1Handle, page, addr, mat1val);
    if (mat_err != ESP_OK) return mat_err;
    mat_err = matSetRegister(sMat2Handle, page, addr, mat2val);
    if (mat_err != ESP_OK) return mat_err;
    mat_err = matSetRegister(sMat3Handle, page, addr, mat3val);
    if (mat_err != ESP_OK) return mat_err;

    return (mat_err_t) ESP_OK;
}

/**
 * @brief Performs I2C transactions to put each of the matrix ICs into the
 * provided setting; ie. modifies bitMask bits to setting in each matrix's
 * config register.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[in] setting The setting.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_MUTEX_TIMEOUT if a timeout occurred while retrieving the device mutex.
 * ESP_ERR_MUTEX if unable to release the device mutex.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * MAT_ERR_UNHANDLED if an internal error was unhandled and the device mutex is
 * in an unknown state.
 * ESP_FAIL if an unexpected error occurred.
 */
static mat_err_t matSetConfig(uint8_t bitMask, uint8_t setting)
{
    mat_err_t mat_err;
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg;

    /* Read current configuration states */
    mat_err = matGetRegisters(&mat1Cfg, &mat2Cfg, &mat3Cfg, CONFIG_PAGE, CONFIG_REG_ADDR);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    /* Generate and set new configuration states */
    matSetBits(&mat1Cfg, bitMask, setting);
    matSetBits(&mat2Cfg, bitMask, setting);
    matSetBits(&mat3Cfg, bitMask, setting);
    mat_err = matSetRegistersSeparate(CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg, mat2Cfg, mat3Cfg);
    if (mat_err == ESP_ERR_INVALID_ARG) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    if (mat_err != ESP_OK) return mat_err;

    return (mat_err_t) ESP_OK;
}

/**
 * @brief Determines and takes the mutex guarding the device.
 * 
 * @requires:
 *   - giveMatrixMutex with the same device handle is called after
 *     the caller is done interacting with the device.
 * 
 * @param[in] device The matrix whose mutex should be taken.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if the device is not a valid matrix handle.
 * ESP_ERR_INVALID_STATE if the device mutex is NULL.
 * ESP_ERR_TIMEOUT if a timeout occurred while retrieving the mutex.
 */
static mat_err_t takeMatrixMutex(i2c_master_dev_handle_t device)
{
    BaseType_t success;
    bool found = false;
    SemaphoreHandle_t mutex = NULL;
    
    /* determine mutex */
    if (device == sMat1Handle)
    {
        found = true;
        mutex = mat1Mutex;
    } else if (device == sMat2Handle)
    {
        found = true;
        mutex = mat2Mutex;
    } else if (device == sMat3Handle)
    {
        found = true;
        mutex = mat3Mutex;
    }
    if (mutex == NULL && !found) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);
    if (mutex == NULL && found) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_STATE);
    
    /* take mutex */
    success = xSemaphoreTake(mutex, portMAX_DELAY);
    if (success != pdTRUE) THROW_MAT_ERR((mat_err_t) ESP_ERR_TIMEOUT);
    return (mat_err_t) ESP_OK;
}

/**
 * @brief Determines and gives the mutex guarding the device.
 * 
 * @requires:
 *  - The calling task must have taken the device mutex with takeMatrixMutex.
 * 
 * @param[in] device The matrix whose mutex should be given.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if the device is not a matrix handle.
 * ESP_ERR_INVALID_STATE if the device mutex is NULL.
 * ESP_FAIL if an error occurred when giving up the mutex, likely because it was
 * not first taken with takeMatrixMutex.
 */
static mat_err_t giveMatrixMutex(i2c_master_dev_handle_t device)
{
    BaseType_t success;
    bool found = false;
    SemaphoreHandle_t mutex = NULL;

    /* determine mutex */
    if (device == sMat1Handle)
    {
        found = true;
        mutex = mat1Mutex;
    } else if (device == sMat2Handle)
    {
        found = true;
        mutex = mat2Mutex;
    } else if (device == sMat3Handle)
    {
        found = true;
        mutex = mat3Mutex;
    }
    if (mutex == NULL && !found) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_ARG);
    if (mutex == NULL && found) THROW_MAT_ERR((mat_err_t) ESP_ERR_INVALID_STATE);

    /* give up mutex */
    success = xSemaphoreGive(mutex);
    if (success != pdTRUE) THROW_MAT_ERR((mat_err_t) ESP_FAIL);
    return (mat_err_t) ESP_OK;
}

/** 
 * @brief Passes through mat_err after conditionally releasing the mutex that
 * may have been aquired from matSetPage.
 * 
 * @note Example Usage:
 * mat_err = matSetPage(device, page);
 * if (mat_err != ESP_OK) return handleMatSetPageErr(mat_err, device);
 * 
 * @param[in] mat_err The error from matSetPage to handle. Must not be ESP_OK.
 * @param[in] device The matrix device whose mutex may need to be released.
 * 
 * @returns The error code from matSetPage if successfully handled.
 * MAT_ERR_MUTEX if the device mutex could not be released.
 * MAT_ERR_UNHANDLED if handling for the error code was unspecified, in which
 * case a system reset is recommended because the state of the mutex is unknown.
 */
static mat_err_t handleMatSetPageErr(mat_err_t mat_err, i2c_master_dev_handle_t device)
{
    switch (mat_err)
    {
        case ESP_ERR_INVALID_ARG:
            /* falls through */
        case ESP_ERR_INVALID_STATE:
            /* falls through */
        case MAT_ERR_MUTEX_TIMEOUT:
            return mat_err;
        case ESP_ERR_INVALID_RESPONSE:
            /* falls through */
        case ESP_ERR_TIMEOUT:
            /* falls through */
        case ESP_FAIL:
            if (giveMatrixMutex(device) != ESP_OK) return MAT_ERR_MUTEX;
            return mat_err;
        case ESP_OK:
            THROW_MAT_ERR((mat_err_t) ESP_FAIL);
        default:
            return MAT_ERR_UNHANDLED; // do not know whether to release mutex or not
    }
}

#endif /* CONFIG_HARDWARE_VERSION == 1 */