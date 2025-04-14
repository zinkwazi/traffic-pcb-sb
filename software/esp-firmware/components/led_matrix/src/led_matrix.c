/**
 * led_matrix.c
 * 
 * This file contains a hardware abstraction layer for interaction with 
 * the LED matrix driver ICs through I2C.
 *
 * See: https://www.lumissil.com/assets/pdf/core/IS31FL3741A_DS.pdf.
 */

#include "led_matrix.h"

#include <stdint.h>

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_debug_helpers.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#include "pinout.h"
#include "app_err.h"

#include "led_registers.h"
#include "led_types.h"

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

static i2c_master_bus_handle_t sI2CBus1 = NULL;
#if CONFIG_HARDWARE_VERSION == 1
/* none */
#elif CONFIG_HARDWARE_VERSION == 2
static i2c_master_bus_handle_t sI2CBus2 = NULL;
#else
#error "Unsupported hardware version!"
#endif

static i2c_master_dev_handle_t sMat1Handle = NULL;
static i2c_master_dev_handle_t sMat2Handle = NULL;
static i2c_master_dev_handle_t sMat3Handle = NULL;
#if CONFIG_HARDWARE_VERSION == 1
/* none */
#elif CONFIG_HARDWARE_VERSION == 2
static i2c_master_dev_handle_t sMat4Handle = NULL;
#else
#error "Unsupported hardware version!"
#endif

static uint8_t sMat1State = UINT8_MAX;
static uint8_t sMat2State = UINT8_MAX;
static uint8_t sMat3State = UINT8_MAX;
#if CONFIG_HARDWARE_VERSION == 1
/* none */
#elif CONFIG_HARDWARE_VERSION == 2
static uint8_t sMat4State = UINT8_MAX;
#else
#error "Unsupported hardware version!"
#endif

/* These mutexes are required in order for the matrix state static variables
above to be stable through multithreading. These mutexes must be obtained before
reading and writing to the state, and must be held throughout the entirety
of a read then write function, otherwise the state may desynchronize from
the actual state of the matrices. The I2C buses themselves are already gaurded
by mutexes internally. */
static SemaphoreHandle_t sMat1Mutex = NULL;
static SemaphoreHandle_t sMat2Mutex = NULL;
static SemaphoreHandle_t sMat3Mutex = NULL;
#if CONFIG_HARDWARE_VERSION == 1
/* none */
#elif CONFIG_HARDWARE_VERSION == 2
static SemaphoreHandle_t sMat4Mutex = NULL;
#else
#error "Unsupported hardware version!"
#endif

static void matSetBits(uint8_t *reg, uint8_t bitMask, uint8_t value);
static esp_err_t matParseLEDRegisterInfo(i2c_master_dev_handle_t *matrixHandle, uint8_t *pwmPage, uint8_t *scalingPage, LEDReg ledReg);
static esp_err_t matSetPage(i2c_master_dev_handle_t device, uint8_t page);
static esp_err_t handleMatSetPageErr(esp_err_t app_err, i2c_master_dev_handle_t device);
static esp_err_t matGetRegister(uint8_t *result, i2c_master_dev_handle_t device, uint8_t page, uint8_t addr);
static esp_err_t matSetRegister(i2c_master_dev_handle_t device, uint8_t page, uint8_t addr, uint8_t data);
static esp_err_t matSetRegisters(uint8_t page, uint8_t addr, uint8_t data);
static esp_err_t matSetConfig(uint8_t bitMask, uint8_t setting);
static esp_err_t takeMatrixMutex(i2c_master_dev_handle_t device);
static esp_err_t giveMatrixMutex(i2c_master_dev_handle_t device);

#if CONFIG_HARDWARE_VERSION == 1
static esp_err_t matGetRegisters(uint8_t *result1, uint8_t *result2, uint8_t *result3, uint8_t page, uint8_t addr);
static esp_err_t matSetRegistersSeparate(uint8_t page, uint8_t addr, uint8_t mat1val, uint8_t mat2val, uint8_t mat3val);
#elif CONFIG_HARDWARE_VERSION == 2
static esp_err_t matGetRegisters(uint8_t *result1, uint8_t *result2, uint8_t *result3, uint8_t *result4, uint8_t page, uint8_t addr);
static esp_err_t matSetRegistersSeparate(uint8_t page, uint8_t addr, uint8_t mat1val, uint8_t mat2val, uint8_t mat3val, uint8_t mat4val);
#endif

/**
 * @brief Initializes the led_matrix component, which allows use of led_matrix.h
 * functions. Resets matrices to normal operating mode.
 * 
 * @returns ESP_OK if successful, otherwise ESP_FAIL.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if the component is already initialized.
 * ESP_ERR_NOT_FOUND if a matrix on the first I2C bus could not be found.
 * ESP_ERR_NO_MEM if not enough memory was available.
 * APP_ERR_MUTEX_RELEASE if a device mutex could not be released. A reboot is
 * recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t initLedMatrix(void)
{
    esp_err_t err;
    /* check that component is not already initialized */
    err = getLedMatrixStatus(); // if not ESP_OK, static vars are now NULL.
    if (err == ESP_OK) THROW_ERR((esp_err_t) ESP_ERR_INVALID_STATE);

    /* initialize component */
#if CONFIG_HARDWARE_VERSION == 1
    err = matInitialize(I2C_PORT, SDA_PIN, SCL_PIN);
    if (err != ESP_OK) return err;
#elif CONFIG_HARDWARE_VERSION == 2
    err = matInitializeBus1(I2C1_PORT, SDA1_PIN, SCL1_PIN);
    if (err != ESP_OK) return err;
    err = matInitializeBus2(I2C2_PORT, SDA2_PIN, SCL2_PIN);
    if (err != ESP_OK) return err;
#else
#error "Unsupported hardware version!"
#endif

    /* confirm proper component initialization */
    err = getLedMatrixStatus();
    if (err != ESP_OK) THROW_ERR(ESP_FAIL);

    /* reset devices */
    err = matReset();
    if (err != ESP_OK) THROW_ERR(ESP_FAIL);
    err = matSetGlobalCurrentControl(CONFIG_GLOBAL_LED_CURRENT);
    if (err != ESP_OK) THROW_ERR(ESP_FAIL);
    err = matSetOperatingMode(NORMAL_OPERATION);
    if (err != ESP_OK) THROW_ERR(ESP_FAIL);

    return (esp_err_t) ESP_OK;
}

/**
 * @brief Checks the initialization status of the led_matrix component.
 * 
 * @returns ESP_OK if the component is initialized.
 * ESP_FAIL if the component is not initialized.
 */
esp_err_t getLedMatrixStatus(void)
{
#if CONFIG_HARDWARE_VERSION == 1
    if (sI2CBus1 == NULL) return ESP_FAIL;
#elif CONFIG_HARDWARE_VERSION == 2
    if (sI2CBus1 == NULL) return ESP_FAIL;
    if (sI2CBus2 == NULL) return ESP_FAIL;
#else
#error "Unsupported hardware version!"
#endif
    return ESP_OK;
}

#if CONFIG_HARDWARE_VERSION == 1
/**
 * @brief Initializes the first I2C bus, which connects to matrix 1 and 2, 
 * asserts that the matrices are connected, and syncs internal state variables
 * to the state of the matrices.
 * 
 * @param[in] port The GPIO port of the first I2C bus.
 * @param[in] sdaPin The GPIO pin of the SDA line.
 * @param[in] sclPin The GPIO pin of the SCL line.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_NOT_FOUND if a matrix on the first I2C bus could not be found.
 * ESP_ERR_NO_MEM if not enough memory was available.
 * APP_ERR_MUTEX_RELEASE if a device mutex could not be released. A reboot is
 * recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matInitialize(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin)
{
    esp_err_t err;
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
    err = i2c_new_master_bus(&master_bus_config, &sI2CBus1);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        if (err == ESP_ERR_NOT_FOUND) THROW_ERR(ESP_FAIL);
        THROW_ERR(err); // ESP_ERR_NO_MEM
    }
    err = i2c_master_bus_add_device(sI2CBus1, &matrix_config, &sMat1Handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        THROW_ERR(err); // ESP_ERR_NO_MEM
    }
    matrix_config.device_address = MAT2_ADDR;
    err = i2c_master_bus_add_device(sI2CBus1, &matrix_config, &sMat2Handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        THROW_ERR(err); // ESP_ERR_NO_MEM
    }
    matrix_config.device_address = MAT3_ADDR;
    err = i2c_master_bus_add_device(sI2CBus1, &matrix_config, &sMat3Handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        THROW_ERR(err); // ESP_ERR_NO_MEM
    }

    /* initialize matrix 1 and 2 mutexes */
    sMat1Mutex = xSemaphoreCreateMutex();
    if (sMat1Mutex == NULL) THROW_ERR(ESP_FAIL);
    sMat2Mutex = xSemaphoreCreateMutex();
    if (sMat2Mutex == NULL) THROW_ERR(ESP_FAIL);
    sMat3Mutex = xSemaphoreCreateMutex();
    if (sMat3Mutex == NULL) THROW_ERR(ESP_FAIL);

    /* initialize matrix state and sync with matrices. This doubles as a check
    that the matrices are connected. */
    sMat1State = PWM0_PAGE; // force setPage to actually set the page
    sMat2State = PWM0_PAGE; // so that pages are synced with hardware
    sMat3State = PWM0_PAGE;
    err = matSetPage(sMat1Handle, CONFIG_PAGE); // acquires sMat1Mutex
    if (err != ESP_OK)
    {
        err = handleMatSetPageErr(err, sMat1Handle); // releases device mutex
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
        if (err == ESP_ERR_INVALID_STATE) return ESP_FAIL;
        if (err == APP_ERR_MUTEX_FAIL) return ESP_FAIL;
        if (err == APP_ERR_UNHANDLED) return ESP_FAIL;
        if (err == ESP_ERR_TIMEOUT) return ESP_ERR_NOT_FOUND;
        if (err == ESP_ERR_INVALID_RESPONSE) return ESP_ERR_NOT_FOUND;
        return err; // APP_ERR_MUTEX_RELEASE
    }
    err = giveMatrixMutex(sMat1Handle);
    if (err != ESP_OK) return APP_ERR_MUTEX_RELEASE;

    err = matSetPage(sMat2Handle, CONFIG_PAGE); // acquires sMat2Mutex
    if (err != ESP_OK)
    {
        err = handleMatSetPageErr(err, sMat2Handle); // releases device mutex
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
        if (err == ESP_ERR_INVALID_STATE) return ESP_FAIL;
        if (err == APP_ERR_MUTEX_FAIL) return ESP_FAIL;
        if (err == APP_ERR_UNHANDLED) return ESP_FAIL;
        if (err == ESP_ERR_TIMEOUT) return ESP_ERR_NOT_FOUND;
        if (err == ESP_ERR_INVALID_RESPONSE) return ESP_ERR_NOT_FOUND;
        return err; // APP_ERR_MUTEX_RELEASE
    }
    err = giveMatrixMutex(sMat2Handle);
    if (err != ESP_OK) return APP_ERR_MUTEX_RELEASE;

    err = matSetPage(sMat3Handle, CONFIG_PAGE); // acquires sMat3Mutex
    if (err != ESP_OK)
    {
        err = handleMatSetPageErr(err, sMat3Handle); // releases device mutex
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
        if (err == ESP_ERR_INVALID_STATE) return ESP_FAIL;
        if (err == APP_ERR_MUTEX_FAIL) return ESP_FAIL;
        if (err == APP_ERR_UNHANDLED) return ESP_FAIL;
        if (err == ESP_ERR_TIMEOUT) return ESP_ERR_NOT_FOUND;
        if (err == ESP_ERR_INVALID_RESPONSE) return ESP_ERR_NOT_FOUND;
        return err; // APP_ERR_MUTEX_RELEASE
    }
    err = giveMatrixMutex(sMat3Handle);
    if (err != ESP_OK) return APP_ERR_MUTEX_RELEASE;

    return (esp_err_t) ESP_OK;
}
#elif CONFIG_HARDWARE_VERSION == 2
/**
 * @brief Initializes the first I2C bus, which connects to matrix 1 and 2, 
 * asserts that the matrices are connected, and syncs internal state variables
 * to the state of the matrices.
 * 
 * @param[in] port The GPIO port of the first I2C bus.
 * @param[in] sdaPin The GPIO pin of the SDA line.
 * @param[in] sclPin The GPIO pin of the SCL line.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_NOT_FOUND if a matrix on the first I2C bus could not be found.
 * ESP_ERR_NO_MEM if not enough memory was available.
 * APP_ERR_MUTEX_RELEASE if a device mutex could not be released. A reboot is
 * recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matInitializeBus1(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin)
{
    esp_err_t err;
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
    err = i2c_new_master_bus(&master_bus_config, &sI2CBus1);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        if (err == ESP_ERR_NOT_FOUND) THROW_ERR(ESP_FAIL);
        THROW_ERR(err); // ESP_ERR_NO_MEM
    }
    err = i2c_master_bus_add_device(sI2CBus1, &matrix_config, &sMat1Handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        THROW_ERR(err); // ESP_ERR_NO_MEM
    }
    matrix_config.device_address = MAT2_ADDR;
    err = i2c_master_bus_add_device(sI2CBus1, &matrix_config, &sMat2Handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        THROW_ERR(err); // ESP_ERR_NO_MEM
    }

    /* initialize matrix 1 and 2 mutexes */
    sMat1Mutex = xSemaphoreCreateMutex();
    if (sMat1Mutex == NULL) THROW_ERR(ESP_FAIL);
    sMat2Mutex = xSemaphoreCreateMutex();
    if (sMat2Mutex == NULL) THROW_ERR(ESP_FAIL);

    /* initialize matrix state and sync with matrices. This doubles as a check
    that the matrices are connected. */
    sMat1State = PWM0_PAGE; // force setPage to actually set the page
    sMat2State = PWM0_PAGE; // so that pages are synced with hardware
    err = matSetPage(sMat1Handle, CONFIG_PAGE); // acquires sMat1Mutex
    if (err != ESP_OK)
    {
        err = handleMatSetPageErr(err, sMat1Handle); // releases device mutex
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
        if (err == ESP_ERR_INVALID_STATE) return ESP_FAIL;
        if (err == APP_ERR_MUTEX_FAIL) return ESP_FAIL;
        if (err == APP_ERR_UNHANDLED) return ESP_FAIL;
        if (err == ESP_ERR_TIMEOUT) return ESP_ERR_NOT_FOUND;
        if (err == ESP_ERR_INVALID_RESPONSE) return ESP_ERR_NOT_FOUND;
        return err; // APP_ERR_MUTEX_RELEASE
    }
    err = giveMatrixMutex(sMat1Handle);
    if (err != ESP_OK) return APP_ERR_MUTEX_RELEASE;

    err = matSetPage(sMat2Handle, CONFIG_PAGE); // acquires sMat1Mutex
    if (err != ESP_OK)
    {
        err = handleMatSetPageErr(err, sMat2Handle); // releases device mutex
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
        if (err == ESP_ERR_INVALID_STATE) return ESP_FAIL;
        if (err == APP_ERR_MUTEX_FAIL) return ESP_FAIL;
        if (err == APP_ERR_UNHANDLED) return ESP_FAIL;
        if (err == ESP_ERR_TIMEOUT) return ESP_ERR_NOT_FOUND;
        if (err == ESP_ERR_INVALID_RESPONSE) return ESP_ERR_NOT_FOUND;
        return err; // APP_ERR_MUTEX_RELEASE
    }
    err = giveMatrixMutex(sMat2Handle);
    if (err != ESP_OK) return APP_ERR_MUTEX_RELEASE;

    return (esp_err_t) ESP_OK;
}

/**
 * @brief Initializes the second I2C bus, which connects to matrix 3 and 4, 
 * asserts that the matrices are connected, and syncs internal state variables
 * to the state of the matrices.
 * 
 * @param[in] port The GPIO port of the second I2C bus.
 * @param[in] sdaPin The GPIO pin of the SDA line.
 * @param[in] sclPin The GPIO pin of the SCL line.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_NOT_FOUND if a matrix on the first I2C bus could not be found.
 * ESP_ERR_NO_MEM if not enough memory was available.
 * APP_ERR_MUTEX_RELEASE if a device mutex could not be released. A reboot is
 * recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matInitializeBus2(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin)
{
    esp_err_t err;
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
        .device_address = MAT3_ADDR,
        .scl_speed_hz = BUS_SPEED_HZ,
        .scl_wait_us = SCL_WAIT_US, // use default value
    };

    /* Initialize I2C bus 1 */
    err = i2c_new_master_bus(&master_bus_config, &sI2CBus2);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        if (err == ESP_ERR_NOT_FOUND) THROW_ERR(ESP_FAIL);
        THROW_ERR(err); // ESP_ERR_NO_MEM
    }
    err = i2c_master_bus_add_device(sI2CBus2, &matrix_config, &sMat3Handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        THROW_ERR(err); // ESP_ERR_NO_MEM
    }
    matrix_config.device_address = MAT4_ADDR;
    err = i2c_master_bus_add_device(sI2CBus2, &matrix_config, &sMat4Handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        THROW_ERR(err); // ESP_ERR_NO_MEM
    }

    /* initialize matrix 1 and 2 mutexes */
    sMat3Mutex = xSemaphoreCreateMutex();
    if (sMat3Mutex == NULL) THROW_ERR(ESP_FAIL);
    sMat4Mutex = xSemaphoreCreateMutex();
    if (sMat4Mutex == NULL) THROW_ERR(ESP_FAIL);

    /* initialize matrix state and sync with matrices. This doubles as a check
    that the matrices are connected. */
    sMat3State = PWM0_PAGE; // force setPage to actually set the page
    sMat4State = PWM0_PAGE; // so that pages are synced with hardware
    err = matSetPage(sMat3Handle, CONFIG_PAGE); // acquires sMat1Mutex
    if (err != ESP_OK)
    {
        err = handleMatSetPageErr(err, sMat3Handle); // releases device mutex
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
        if (err == ESP_ERR_INVALID_STATE) return ESP_FAIL;
        if (err == APP_ERR_MUTEX_FAIL) return ESP_FAIL;
        if (err == APP_ERR_UNHANDLED) return ESP_FAIL;
        if (err == ESP_ERR_TIMEOUT) return ESP_ERR_NOT_FOUND;
        if (err == ESP_ERR_INVALID_RESPONSE) return ESP_ERR_NOT_FOUND;
        return err; // APP_ERR_MUTEX_RELEASE
    }
    err = giveMatrixMutex(sMat3Handle);
    if (err != ESP_OK) return APP_ERR_MUTEX_RELEASE;

    err = matSetPage(sMat4Handle, CONFIG_PAGE); // acquires sMat1Mutex
    if (err != ESP_OK)
    {
        err = handleMatSetPageErr(err, sMat4Handle); // releases device mutex
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
        if (err == ESP_ERR_INVALID_STATE) return ESP_FAIL;
        if (err == APP_ERR_MUTEX_FAIL) return ESP_FAIL;
        if (err == APP_ERR_UNHANDLED) return ESP_FAIL;
        if (err == ESP_ERR_TIMEOUT) return ESP_ERR_NOT_FOUND;
        if (err == ESP_ERR_INVALID_RESPONSE) return ESP_ERR_NOT_FOUND;
        return err; // APP_ERR_MUTEX_RELEASE
    }
    err = giveMatrixMutex(sMat4Handle);
    if (err != ESP_OK) return APP_ERR_MUTEX_RELEASE;

    return (esp_err_t) ESP_OK;
}
#else
#error "Unsupported hardware version!"
#endif /* CONFIG_HARDWARE_VERSION */

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
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matSetOperatingMode(enum Operation setting)
{
    if (setting >= MATRIX_OPERATION_MAX) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);
    return (esp_err_t) matSetConfig(SOFTWARE_SHUTDOWN_BITS, (uint8_t) setting);
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
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matSetOpenShortDetection(enum ShortDetectionEnable setting)
{
    if (setting >= MATRIX_SHORT_DETECTION_EN_MAX) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);
    return (esp_err_t) matSetConfig(OPEN_SHORT_DETECT_EN_BITS, (uint8_t) setting);
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
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matSetLogicLevel(enum LogicLevel setting)
{
    if (setting >= MATRIX_LOGIC_LEVEL_MAX) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);
    return (esp_err_t) matSetConfig(LOGIC_LEVEL_CNTRL_BITS, (uint8_t) setting);
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
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matSetSWxSetting(enum SWXSetting setting)
{
    if (setting >= MATRIX_SWXSETTING_MAX) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);
    return (esp_err_t) matSetConfig(SWX_SETTING_BITS, (uint8_t) setting);
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
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matSetGlobalCurrentControl(uint8_t value)
{
    esp_err_t err;

    err = matSetRegisters(CONFIG_PAGE, CURRENT_CNTRL_REG_ADDR, value);
    if (err == ESP_ERR_INVALID_ARG) THROW_ERR((esp_err_t) ESP_FAIL);
    return (esp_err_t) err;
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
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matSetResistorPullupSetting(enum ResistorSetting setting)
{
    if (setting >= MATRIX_RESISTORSETTING_MAX) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);
    return (esp_err_t) matSetConfig(PUR_BITS, (uint8_t) setting);
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
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matSetResistorPulldownSetting(enum ResistorSetting setting)
{
    if (setting >= MATRIX_RESISTORSETTING_MAX) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);
    return (esp_err_t) matSetConfig(PDR_BITS, (uint8_t) setting);
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
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matSetPWMFrequency(enum PWMFrequency freq)
{
    esp_err_t err;
    /* input guards */
    if (freq >= MATRIX_PWMFREQ_MAX) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);

    /* set registers */
    err = matSetRegisters(CONFIG_PAGE, PWM_FREQ_REG_ADDR, (uint8_t) freq);
    if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
    return (esp_err_t) err;
}

/**
 * @brief Resets all matrix registers to default values.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @returns ESP_OK if successful. Otherwise, the target register may have
 * changed in one or multiple matrices, but not all.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matReset(void)
{
    esp_err_t err;

    /* set registers */
    err = matSetRegisters(CONFIG_PAGE, RESET_REG_ADDR, RESET_KEY);
    if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
    return (esp_err_t) err;
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
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_INVALID_PAGE if LEDNumToReg[ledNum] has an invalid associated page.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matSetColor(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t err;
    LEDReg ledReg;
    i2c_master_dev_handle_t matrixHandle;
    uint8_t page;
    /* input guards */
    if (ledNum == 0) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (ledNum > MAX_NUM_LEDS_REG) THROW_ERR(ESP_ERR_INVALID_ARG);

    /* determine the correct PWM registers */
    ledReg = LEDNumToReg[ledNum - 1];
    err = matParseLEDRegisterInfo(&matrixHandle, &page, NULL, ledReg);
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;

    /* set PWM registers to provided values */
    err = matSetRegister(matrixHandle, page, ledReg.red, red);
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;

    err = matSetRegister(matrixHandle, page, ledReg.green, green);
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;

    err = matSetRegister(matrixHandle, page, ledReg.blue, blue);
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;

    return (esp_err_t) ESP_OK;
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
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_INVALID_PAGE if LEDNumToReg[ledNum] has an invalid associated page.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
esp_err_t matSetScaling(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue)
{
    /* guard input */
    esp_err_t err;
    LEDReg ledReg;
    i2c_master_dev_handle_t matrixHandle;
    uint8_t page;
    /* input guards */
    if (ledNum == 0) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (ledNum > MAX_NUM_LEDS_REG) THROW_ERR(ESP_ERR_INVALID_ARG);

    /* determine the correct PWM registers */
    ledReg = LEDNumToReg[ledNum - 1];
    err = matParseLEDRegisterInfo(&matrixHandle, NULL, &page, ledReg);
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;

    /* set PWM registers to provided values */
    err = matSetRegister(matrixHandle, page, ledReg.red, red);
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;

    err = matSetRegister(matrixHandle, page, ledReg.green, green);
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;

    err = matSetRegister(matrixHandle, page, ledReg.blue, blue);
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK) return err;

    return (esp_err_t) ESP_OK;
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
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t matSetConfig(uint8_t bitMask, uint8_t setting)
{
    esp_err_t err;
#if CONFIG_HARDWARE_VERSION == 1
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg;
#elif CONFIG_HARDWARE_VERSION == 2
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg, mat4Cfg;
#else
#error "Unsupported hardware version!"
#endif

    /* Read current configuration states */
#if CONFIG_HARDWARE_VERSION == 1
    err = matGetRegisters(&mat1Cfg, &mat2Cfg, &mat3Cfg, CONFIG_PAGE, CONFIG_REG_ADDR);
#elif CONFIG_HARDWARE_VERSION == 2
    err = matGetRegisters(&mat1Cfg, &mat2Cfg, &mat3Cfg, &mat4Cfg, CONFIG_PAGE, CONFIG_REG_ADDR);
#else
#error "Unsupported hardware version!"
#endif
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
        return err;
    }

    /* Generate and set new configuration states */
    matSetBits(&mat1Cfg, bitMask, setting);
    matSetBits(&mat2Cfg, bitMask, setting);
    matSetBits(&mat3Cfg, bitMask, setting);
#if CONFIG_HARDWARE_VERSION == 1
err = matSetRegistersSeparate(CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg, mat2Cfg, mat3Cfg);
#elif CONFIG_HARDWARE_VERSION == 2
    matSetBits(&mat4Cfg, bitMask, setting);
    err = matSetRegistersSeparate(CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg, mat2Cfg, mat3Cfg, mat4Cfg);
#else
#error "Unsupported hardware version!"
#endif
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
        return err;
    }

    return (esp_err_t) ESP_OK;
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
 * APP_ERR_INVALID_PAGE if ledReg.matrix is invalid.
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t matParseLEDRegisterInfo(i2c_master_dev_handle_t *matrixHandle, uint8_t *pwmPage, uint8_t *scalingPage, LEDReg ledReg)
{
    /* input guards */
    if (matrixHandle == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (pwmPage == NULL && scalingPage == NULL) THROW_ERR(ESP_ERR_INVALID_ARG); // the function assumes at least one of the pages should be returned
    if (ledReg.matrix >= MAT_NONE) THROW_ERR((esp_err_t) APP_ERR_INVALID_PAGE);

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
#if CONFIG_HARDWARE_VERSION == 1
    /* none */
#elif CONFIG_HARDWARE_VERSION == 2
    case MAT4_PAGE0:
        *matrixHandle = sMat4Handle;
        if (pwmPage != NULL)
        {
            *pwmPage = PWM0_PAGE;
        }
        if (scalingPage != NULL)
        {
            *scalingPage = SCALING0_PAGE;
        }
        break;
    case MAT4_PAGE1:
        *matrixHandle = sMat4Handle;
        if (pwmPage != NULL)
        {
            *pwmPage = PWM1_PAGE;
        }
        if (scalingPage != NULL)
        {
            *scalingPage = SCALING1_PAGE;
        }
        break;
#else
#error "Unsupported hardware version!"
#endif
    default:
        THROW_ERR((esp_err_t) ESP_FAIL);
    }
    return (esp_err_t) ESP_OK;
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
 * with the device is complete, unless ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE,
 * or APP_ERR_MUTEX_FAIL a
 * 
 * @param[in] device The matrix IC device handle.
 * @param[in] page The target page.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if the function requirements are not met.
 * ESP_ERR_TIMEOUT if the I2C bus timed out while waiting for a response.
 * ESP_ERR_INVALID_RESPONSE if the device did not respond with the provided key.
 * APP_ERR_MUTEX_FAIL if a problem occurred while acquiring the device mutex.
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t matSetPage(i2c_master_dev_handle_t device, uint8_t page)
{
    esp_err_t err;
    uint8_t buffer[2];
    /* input guards */
    if (page > 4) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);
    // device argument validated by takeMatrixMutexs

    /* retrieve device mutex */
    err = takeMatrixMutex(device); // caller must release mutex bc there are many return paths
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) return APP_ERR_MUTEX_FAIL;
        if (err == ESP_ERR_TIMEOUT) return APP_ERR_MUTEX_FAIL;
        return (esp_err_t) err; // ESP_ERR_INVALID_STATE
    }

    /* check current page setting */
    err = ESP_FAIL;
    if (device == sMat1Handle && page == sMat1State)
    {
        err = ESP_OK;
    } else if (device == sMat2Handle && page == sMat2State)
    {
        err = ESP_OK;
    } else if (device == sMat3Handle && page == sMat3State)
    {
        err = ESP_OK;
#if CONFIG_HARDWARE_VERSION == 1
    }
#elif CONFIG_HARDWARE_VERSION == 2
    } else if (device == sMat4Handle && page == sMat4State)
    {
        err = ESP_OK;
    }
#else
#error "Unsupported hardware version!"
#endif
    if (err == ESP_OK) return ESP_OK; // the device is on the target page already

    /* unlock command register */
    FILL_BUFFER(CMD_REG_WRITE_LOCK_ADDR, CMD_REG_WRITE_KEY);
    err = i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS);
    if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
    if (err != ESP_OK) THROW_ERR(err); // ESP_ERR_TIMEOUT

    /* confirm unlocked command register */
    FILL_BUFFER(CMD_REG_WRITE_LOCK_ADDR, 0);
    err = i2c_master_transmit_receive(device, &(buffer[0]), 1, &(buffer[1]), 1, I2C_TIMEOUT_MS);
    if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
    if (err != ESP_OK) THROW_ERR(err); // ESP_ERR_TIMEOUT

    if (buffer[1] != CMD_REG_WRITE_KEY) THROW_ERR((esp_err_t) ESP_ERR_INVALID_RESPONSE);

    /* update page */
    FILL_BUFFER(CMD_REG_ADDR, page);
    err = i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS);
    if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
    if (err != ESP_OK) THROW_ERR(err); // ESP_ERR_TIMEOUT

    if (device == sMat1Handle)
    {
        sMat1State = page;
    } else if (device == sMat2Handle)
    {
        sMat2State = page;
    } else if (device == sMat3Handle)
    {
        sMat3State = page;
#if CONFIG_HARDWARE_VERSION == 1
    } else
#elif CONFIG_HARDWARE_VERSION == 2
    } else if (device == sMat4Handle)
    {
        sMat4State = page;
    } else
#else
#error "Unsupported hardware version!"
#endif
    {
        /* The device was previously verified as a matrix handle */
        THROW_ERR(ESP_FAIL);
    }
    return ESP_OK;
}

/** 
 * @brief Passes through app_err after conditionally releasing the mutex that
 * may have been aquired from matSetPage.
 * 
 * @note Example Usage:
 * app_err = matSetPage(device, page);
 * if (app_err != ESP_OK) return handleMatSetPageErr(app_err, device);
 * 
 * @param[in] err The error from matSetPage to handle. Must not be ESP_OK.
 * @param[in] device The matrix device whose mutex may need to be released.
 * 
 * @returns The error code from matSetPage if successfully handled.
 * APP_ERR_MUTEX_RELEASE if an error occurred while releasing the mutex.
 * APP_ERR_UNHANDLED if matSetPage threw an error that this function did not handle.
 */
static esp_err_t handleMatSetPageErr(esp_err_t err, i2c_master_dev_handle_t device)
{
    switch (err)
    {
        case ESP_ERR_INVALID_ARG:
            /* falls through */
        case ESP_ERR_INVALID_STATE:
            /* falls through */
        case APP_ERR_MUTEX_FAIL:
            return (esp_err_t) err; // don't need to release mutex
        case ESP_ERR_INVALID_RESPONSE:
            /* falls through */
        case ESP_ERR_TIMEOUT:
            /* falls through */
        case ESP_FAIL:
            if (giveMatrixMutex(device) != ESP_OK) return (esp_err_t) APP_ERR_MUTEX_RELEASE;
            return err;
        case ESP_OK:
            THROW_ERR(ESP_FAIL);
        default:
            break;
    }
    return (esp_err_t) APP_ERR_UNHANDLED; // do not know whether to release mutex or not
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
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t matGetRegister(uint8_t *result, i2c_master_dev_handle_t device, uint8_t page, uint8_t addr)
{
    esp_err_t err, err2;
    /* input guards */
    if (result == NULL) THROW_ERR(ESP_ERR_INVALID_ARG);
    if (page > 4) THROW_ERR(ESP_ERR_INVALID_ARG);
    // device validated by matSetPage

    /* Set page and read config registers */
    err = matSetPage(device, page); // acquires device mutex
    if (err != ESP_OK)
    {
        err = handleMatSetPageErr(err, device); // releases device mutex
        if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
        if (err == APP_ERR_MUTEX_FAIL) return ESP_FAIL;
        if (err == APP_ERR_UNHANDLED) return ESP_FAIL;
        if (err == ESP_ERR_TIMEOUT) return ESP_ERR_TIMEOUT;
        if (err == APP_ERR_MUTEX_RELEASE) return APP_ERR_MUTEX_RELEASE;
        return (esp_err_t) err; // ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_RESPONSE
    }

    if (err != ESP_OK) return (esp_err_t) err;
    err = i2c_master_transmit_receive(device, &addr, 1, result, 1, I2C_TIMEOUT_MS);
    // err handled after giving up device mutex
    err2 = giveMatrixMutex(device);
    if (err2 != ESP_OK) return APP_ERR_MUTEX_RELEASE;
    if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        THROW_ERR((esp_err_t) err); // ESP_ERR_TIMEOUT
    }
    return (esp_err_t) ESP_OK;
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
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if unable to release the device mutex. A reboot is
 * recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t matSetRegister(i2c_master_dev_handle_t device, uint8_t page, uint8_t addr, uint8_t data)
{
    esp_err_t err1, err2;
    uint8_t buffer[2];
    /* gaurd input */
    // device and page are validated by matSetPage

    /* move to the correct page */
    err1 = matSetPage(device, page); // acquires device mutex
    if (err1 != ESP_OK)
    {
        err1 = handleMatSetPageErr(err1, device); // releases device mutex
        if (err1 == ESP_ERR_INVALID_ARG) return ESP_FAIL;
        if (err1 == APP_ERR_MUTEX_FAIL) return ESP_FAIL;
        if (err1 == APP_ERR_UNHANDLED) return ESP_FAIL;
        if (err1 == ESP_ERR_TIMEOUT) return ESP_ERR_TIMEOUT;
        if (err1 == APP_ERR_MUTEX_RELEASE) return APP_ERR_MUTEX_RELEASE;
        return (esp_err_t) err1; // ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_RESPONSE, ESP_ERR_INVALID_ARG
    }

    /* transmit message */
    FILL_BUFFER(addr, data);
    err1 = i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS);
    // err1 handled after giving up device mutex
    err2 = giveMatrixMutex(device);
    if (err2 != ESP_OK) return APP_ERR_MUTEX_RELEASE;
    if (err1 != ESP_OK)
    {
        if (err1 == ESP_ERR_INVALID_ARG) THROW_ERR(ESP_FAIL);
        THROW_ERR(err1); // ESP_ERR_TIMEOUT
    }

    return (esp_err_t) ESP_OK;
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
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if unable to release the device mutex. A reboot is
 * recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t matSetRegisters(uint8_t page, uint8_t addr, uint8_t data)
{
    esp_err_t err;
    /* guard input */
    if (page > 4) THROW_ERR(ESP_ERR_INVALID_ARG);

    /* set registers */
    err = matSetRegister(sMat1Handle, page, addr, data);
    if (err != ESP_OK) return err;
    err = matSetRegister(sMat2Handle, page, addr, data);
    if (err != ESP_OK) return err;
    err = matSetRegister(sMat3Handle, page, addr, data);
    if (err != ESP_OK) return err;
#if CONFIG_HARDWARE_VERSION == 1
    /* none */
#elif CONFIG_HARDWARE_VERSION == 2
    err = matSetRegister(sMat4Handle, page, addr, data);
    if (err != ESP_OK) return err;
#else
#error "Unsupported hardware version!"
#endif

    return (esp_err_t) ESP_OK;
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
static esp_err_t takeMatrixMutex(i2c_master_dev_handle_t device)
{
    BaseType_t success;
    bool found = false;
    SemaphoreHandle_t mutex = NULL;
    
    /* determine mutex */
    if (device == sMat1Handle)
    {
        found = true;
        mutex = sMat1Mutex;
    } else if (device == sMat2Handle)
    {
        found = true;
        mutex = sMat2Mutex;
    } else if (device == sMat3Handle)
    {
        found = true;
        mutex = sMat3Mutex;
#if CONFIG_HARDWARE_VERSION == 1
    }
#elif CONFIG_HARDWARE_VERSION == 2
    } else if (device == sMat4Handle)
    {
        found = true;
        mutex = sMat4Mutex;
    }
#else
#error "Unsupported hardware version!"
#endif
    if (mutex == NULL && !found) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);
    if (mutex == NULL && found) THROW_ERR((esp_err_t) ESP_ERR_INVALID_STATE);
    
    /* take mutex */
    success = xSemaphoreTake(mutex, portMAX_DELAY);
    if (success != pdTRUE) THROW_ERR((esp_err_t) ESP_ERR_TIMEOUT);
    return (esp_err_t) ESP_OK;
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
static esp_err_t giveMatrixMutex(i2c_master_dev_handle_t device)
{
    BaseType_t success;
    bool found = false;
    SemaphoreHandle_t mutex = NULL;

    /* determine mutex */
    if (device == sMat1Handle)
    {
        found = true;
        mutex = sMat1Mutex;
    } else if (device == sMat2Handle)
    {
        found = true;
        mutex = sMat2Mutex;
    } else if (device == sMat3Handle)
    {
        found = true;
        mutex = sMat3Mutex;
#if CONFIG_HARDWARE_VERSION == 1
    }
#elif CONFIG_HARDWARE_VERSION == 2
    } else if (device == sMat4Handle)
    {
        found = true;
        mutex = sMat4Mutex;
    }
#else
#error "Unsupported hardware version!"
#endif
    if (mutex == NULL && !found) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);
    if (mutex == NULL && found) THROW_ERR((esp_err_t) ESP_ERR_INVALID_STATE);

    /* give up mutex */
    success = xSemaphoreGive(mutex);
    if (success != pdTRUE) THROW_ERR((esp_err_t) ESP_FAIL);
    return (esp_err_t) ESP_OK;
}


#if CONFIG_HARDWARE_VERSION == 1
/**
 * @brief Retrieves the data at the target register for all matrices.
 * 
 * @requires:
 * - I2C bus/buses initialized with matInitialize... functions.
 * 
 * @param[out] result1 The location to place the register value in matrix 1.
 * @param[out] result2 The location to place the register value in matrix 2.
 * @param[out] result3 The location to place the register value in matrix 3.
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t matGetRegisters(uint8_t *result1, uint8_t *result2, uint8_t *result3, uint8_t page, uint8_t addr)
{
    esp_err_t err;
    uint8_t localRes1, localRes2, localRes3; // if something fails, do not modify results
    /* guard input */
    if (page > 4) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);

    /* get registers */
    if (result1 != NULL)
    {
        err = matGetRegister(&localRes1, sMat1Handle, page, addr);
        if (err != ESP_OK)
        {
            if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
            return err;
        }
    }
    if (result2 != NULL)
    {
        err = matGetRegister(&localRes2, sMat2Handle, page, addr);
        if (err != ESP_OK)
        {
            if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
            return err;
        }
    }
    if (result3 != NULL)
    {
        err = matGetRegister(&localRes3, sMat3Handle, page, addr);
        if (err != ESP_OK)
        {
            if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
            return err;
        }
    }
    
    /* output results */
    if (result1 != NULL) *result1 = localRes1;
    if (result2 != NULL) *result2 = localRes2;
    if (result3 != NULL) *result3 = localRes3;
    return (esp_err_t) ESP_OK;
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
 * 
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if unable to release the device mutex. A reboot is
 * recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t matSetRegistersSeparate(uint8_t page, uint8_t addr, uint8_t mat1val, uint8_t mat2val, uint8_t mat3val)
{
    esp_err_t err;
    /* guard input */
    if (page > 4) THROW_ERR(ESP_ERR_INVALID_ARG);

    /* set registers */
    err = matSetRegister(sMat1Handle, page, addr, mat1val);
    if (err != ESP_OK) return err;
    err = matSetRegister(sMat2Handle, page, addr, mat2val);
    if (err != ESP_OK) return err;
    err = matSetRegister(sMat3Handle, page, addr, mat3val);
    if (err != ESP_OK) return err;

    return (esp_err_t) ESP_OK;
}
#elif CONFIG_HARDWARE_VERSION == 2
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
 * ESP_ERR_INVALID_ARG if invalid argument.
 * ESP_ERR_INVALID_STATE if requirement 1 is not met.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if the device mutex could not be released and a reboot
 * is recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t matGetRegisters(uint8_t *result1, uint8_t *result2, uint8_t *result3, uint8_t *result4, uint8_t page, uint8_t addr)
{
    esp_err_t err;
    uint8_t localRes1, localRes2, localRes3, localRes4; // if something fails, do not modify results
    /* guard input */
    if (page > 4) THROW_ERR((esp_err_t) ESP_ERR_INVALID_ARG);

    /* get registers */
    if (result1 != NULL)
    {
        err = matGetRegister(&localRes1, sMat1Handle, page, addr);
        if (err != ESP_OK)
        {
            if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
            return err;
        }
    }
    if (result2 != NULL)
    {
        err = matGetRegister(&localRes2, sMat2Handle, page, addr);
        if (err != ESP_OK)
        {
            if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
            return err;
        }
    }
    if (result3 != NULL)
    {
        err = matGetRegister(&localRes3, sMat3Handle, page, addr);
        if (err != ESP_OK)
        {
            if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
            return err;
        }
    }
    if (result4 != NULL)
    {
        err = matGetRegister(&localRes4, sMat4Handle, page, addr);
        if (err != ESP_OK)
        {
            if (err == ESP_ERR_INVALID_ARG) return ESP_FAIL;
            return err;
        }
    }
    
    /* output results */
    if (result1 != NULL) *result1 = localRes1;
    if (result2 != NULL) *result2 = localRes2;
    if (result3 != NULL) *result3 = localRes3;
    if (result4 != NULL) *result4 = localRes4;
    return (esp_err_t) ESP_OK;
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
 * @returns ESP_OK if successful.
 * ESP_ERR_INVALID_ARG if invalid arguments.
 * ESP_ERR_INVALID_STATE if requirement 1 is unsatisfied.
 * ESP_ERR_TIMEOUT if an I2C transaction timed out.
 * ESP_ERR_INVALID_RESPONSE if the matrix page could not be set properly.
 * APP_ERR_MUTEX_RELEASE if unable to release the device mutex. A reboot is
 * recommended.
 * ESP_FAIL if an unexpected error occurred.
 */
static esp_err_t matSetRegistersSeparate(uint8_t page, uint8_t addr, uint8_t mat1val, uint8_t mat2val, uint8_t mat3val, uint8_t mat4val)
{
    esp_err_t err;
    /* guard input */
    if (page > 4) THROW_ERR(ESP_ERR_INVALID_ARG);

    /* set registers */
    err = matSetRegister(sMat1Handle, page, addr, mat1val);
    if (err != ESP_OK) return err;
    err = matSetRegister(sMat2Handle, page, addr, mat2val);
    if (err != ESP_OK) return err;
    err = matSetRegister(sMat3Handle, page, addr, mat3val);
    if (err != ESP_OK) return err;
    err = matSetRegister(sMat4Handle, page, addr, mat4val);
    if (err != ESP_OK) return err;

    return (esp_err_t) ESP_OK;
}
#else
#error "Unsupported hardware version!"
#endif