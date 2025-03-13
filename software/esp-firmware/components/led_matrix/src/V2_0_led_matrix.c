/**
 * V2_0_led_matrix.c
 * 
 * This file contains a hardware abstraction layer for interaction with 
 * the LED matrix driver ICs through two I2C buses on V2_0.
 *
 * See: https://www.lumissil.com/assets/pdf/core/IS31FL3741A_DS.pdf.
 */

#include "sdkconfig.h"

#if CONFIG_HARDWARE_VERSION == 2

#include "led_matrix.h"

#include <stdint.h>

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include "esp_log.h"

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

static i2c_master_bus_handle_t sI2CBus1;
static i2c_master_bus_handle_t sI2CBus2;
static i2c_master_dev_handle_t sMat1Handle;
static i2c_master_dev_handle_t sMat2Handle;
static i2c_master_dev_handle_t sMat3Handle;
static i2c_master_dev_handle_t sMat4Handle;

static uint8_t sMat1State;
static uint8_t sMat2State;
static uint8_t sMat3State;
static uint8_t sMat4State;

esp_err_t matAssertConnectedBus1(void);
esp_err_t matAssertConnectedBus2(void);
esp_err_t matSetPage(i2c_master_dev_handle_t device, uint8_t page);
esp_err_t matGetRegister(uint8_t *result, i2c_master_dev_handle_t device, uint8_t page, uint8_t addr);

esp_err_t matInitializeBus1(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin)
{
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
    if (i2c_new_master_bus(&master_bus_config, &sI2CBus1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (i2c_master_bus_add_device(sI2CBus1, &matrix_config, &sMat1Handle) != ESP_OK)
    {
        return ESP_FAIL;
    }
    matrix_config.device_address = MAT2_ADDR;
    if (i2c_master_bus_add_device(sI2CBus1, &matrix_config, &sMat2Handle) != ESP_OK)
    {
        return ESP_FAIL;
    }
    /* Assert matrices are connected */
    if (matAssertConnectedBus1() != ESP_OK)
    {
        return ESP_FAIL;
    }
    sMat1State = PWM0_PAGE; // force setPage to actually set the page
    sMat2State = PWM0_PAGE; // so that pages are in a known state
    if (matSetPage(sMat1Handle, CONFIG_PAGE) != ESP_OK ||
        matSetPage(sMat2Handle, CONFIG_PAGE) != ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t matInitializeBus2(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin)
{
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
    if (i2c_new_master_bus(&master_bus_config, &sI2CBus2) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (i2c_master_bus_add_device(sI2CBus2, &matrix_config, &sMat3Handle) != ESP_OK)
    {
        return ESP_FAIL;
    }
    matrix_config.device_address = MAT4_ADDR;
    if (i2c_master_bus_add_device(sI2CBus2, &matrix_config, &sMat4Handle) != ESP_OK)
    {
        return ESP_FAIL;
    }
    /* Assert matrices are connected */
    if (matAssertConnectedBus2() != ESP_OK)
    {
        return ESP_FAIL;
    }
    sMat3State = PWM0_PAGE; // force setPage to actually set the page
    sMat4State = PWM0_PAGE; // so that pages are in a known state
    if (matSetPage(sMat3Handle, CONFIG_PAGE) != ESP_OK ||
        matSetPage(sMat4Handle, CONFIG_PAGE) != ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
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
 * @returns ESP_OK if successfully populated matrixHandle and page, otherwise
 *          ESP_FAIL.
 */
esp_err_t matParseLEDRegisterInfo(i2c_master_dev_handle_t *matrixHandle, uint8_t *pwmPage, uint8_t *scalingPage, LEDReg ledReg)
{
    /* input guards */
    if (matrixHandle == NULL)
    {
        return ESP_FAIL;
    }
    if (pwmPage == NULL && scalingPage == NULL)
    {
        // the function assumes at least one of the pages should be returned
        return ESP_FAIL;
    }
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
    default:
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t matAssertConnectedBus1(void)
{
    uint8_t id = 0;
    /* input guards */
    if (sMat1Handle == NULL ||
        sMat2Handle == NULL)
    {
        return ESP_FAIL;
    }
    /* probe matrix 1 */
    esp_err_t err = i2c_master_probe(sI2CBus1, MAT1_ADDR, PROBE_WAIT_MS);
    if (err != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (matGetRegister(&id, sMat1Handle, 1, ID_REG_ADDR) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (id != MAT1_ADDR << 1)
    { // left shift for r/w bit
        return ESP_FAIL;
    }
    /* probe matrix 2 */
    if (i2c_master_probe(sI2CBus1, MAT2_ADDR, PROBE_WAIT_MS) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (matGetRegister(&id, sMat2Handle, 1, ID_REG_ADDR) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (id != MAT2_ADDR << 1)
    { // left shift for r/w bit
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t matAssertConnectedBus2(void)
{
    uint8_t id = 0;
    /* input guards */
    if (sMat3Handle == NULL ||
        sMat4Handle == NULL)
    {
        return ESP_FAIL;
    }
    /* probe matrix 3 */
    if (i2c_master_probe(sI2CBus2, MAT3_ADDR, PROBE_WAIT_MS) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (matGetRegister(&id, sMat3Handle, 1, ID_REG_ADDR) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (id != MAT1_ADDR << 1)
    { // left shift for r/w bit
        return ESP_FAIL;
    }
    /* probe matrix 4 */
    if (i2c_master_probe(sI2CBus2, MAT4_ADDR, PROBE_WAIT_MS) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (matGetRegister(&id, sMat4Handle, 1, ID_REG_ADDR) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (id != MAT2_ADDR << 1)
    { // left shift for r/w bit
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * Sets the bits denoted by bitMask to value in reg. Meant to be
 * used to update configuration bits in matrix registers.
 *
 * Parameters:
 *  - reg: The register to be changed.
 *  - bitMask: The bits to be changed to value.
 *  - value: The value to change bitMask bits in reg to. If
 *           greater than what bitMask can contain, this value
 *           will silently be shortened.
 */
void matSetBits(uint8_t *reg, uint8_t bitMask, uint8_t value)
{
    /* Align value to bitMask */
    for (uint8_t currShift = 0; currShift < 8; currShift++)
    {
        if (bitMask % (0x01 << currShift) == 0x00)
        {
            break;
        }
        value <<= currShift;
    }
    /* Update prev mask bits */
    *reg &= ~bitMask;          // clear previous mask bits
    *reg |= (bitMask & value); // set value to mask bits of reg
}

/**
 * Changes the current matrix device register page by unlocking
 * and writing to the command register via I2C.
 *
 * If the saved state in the ESP32 (state) indicates that
 * the device is already in the requested page, the function
 * will return ESP_OK without performing any I2C transactions.
 */
esp_err_t matSetPage(i2c_master_dev_handle_t device, uint8_t page)
{
    uint8_t buffer[2];
    /* input guards */
    if (page > 4 || device == NULL)
    {
        return ESP_FAIL;
    }
    /* check current page setting */
    if (device == sMat1Handle && page == sMat1State)
    {
        return ESP_OK;
    }
    if (device == sMat2Handle && page == sMat2State)
    {
        return ESP_OK;
    }
    if (device == sMat3Handle && page == sMat3State)
    {
        return ESP_OK;
    }
    if (device == sMat4Handle && page == sMat4State)
    {
        return ESP_OK;
    }
    /* unlock command register */
    FILL_BUFFER(CMD_REG_WRITE_LOCK_ADDR, CMD_REG_WRITE_KEY);
    if (i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS) != ESP_OK)
    {
        return ESP_FAIL;
    }
    /* confirm unlocked command register */
    FILL_BUFFER(CMD_REG_WRITE_LOCK_ADDR, 0);
    if (i2c_master_transmit_receive(device, &(buffer[0]), 1, &(buffer[1]), 1, I2C_TIMEOUT_MS) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (buffer[1] != CMD_REG_WRITE_KEY)
    {
        return ESP_FAIL;
    }
    /* update page */
    FILL_BUFFER(CMD_REG_ADDR, page);
    if (i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (device == sMat1Handle)
    {
        sMat1State = page;
    }
    if (device == sMat2Handle)
    {
        sMat2State = page;
    }
    if (device == sMat3Handle)
    {
        sMat3State = page;
    }
    if (device == sMat4Handle)
    {
        sMat4State = page;
    }
    return ESP_OK;
}

/**
 * Performs I2C transactions to move matrix IC pages to the desired
 * page and reads the IC response of requesting data at the address.
 *
 * Parameters:
 *  - result: The location to store the result of the register read.
 *  - device: The i2c handle of the matrix IC.
 *  - page: The page the target register exists in.
 *  - addr: The address of the target register.
 */
esp_err_t matGetRegister(uint8_t *result, i2c_master_dev_handle_t device, uint8_t page, uint8_t addr)
{
    /* input guards */
    if (result == NULL)
    {
        return ESP_FAIL;
    }
    /* Set page and read config registers */
    matSetPage(device, page);
    esp_err_t err = i2c_master_transmit_receive(device, &addr, 1, result, 1, I2C_TIMEOUT_MS);
    if (err != ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * Gets the data at the register for all matrix ICs, moving
 * each current page to the desired page and reads the response
 * of each matrix to requesting data at the address.
 *
 * Parameters:
 *  - result1: The location to store the result of the matrix 1
 *             register read. If NULL, matrix 1 is skipped.
 *  - result2: The location to store the result of the matrix 2
 *             register read. If NULL, matrix 2 is skipped.
 *  - result3: The location to store the result of the matrix 3
 *             register read. If NULL, matrix 3 is skipped.
 *  - result4: The location to store the result of the matrix 4
 *             register read. If NULL, matrix 4 is skipped.
 *  - page: The page the target register exists in.
 *  - addr: The address of the target register.
 *
 * Returns: ESP_OK if successful. Otherwise, the data pointed to
 *          by the results are unmodified, however pages may
 *          have been modified.
 */
esp_err_t matGetRegisters(uint8_t *result1, uint8_t *result2, uint8_t *result3, uint8_t *result4, uint8_t page, uint8_t addr)
{
    uint8_t localRes1, localRes2, localRes3, localRes4; // if something fails, do not modify results
    /* guard input */
    if (page > 4)
    {
        return ESP_FAIL;
    }
    /* get registers */
    if (result1 != NULL && matGetRegister(&localRes1, sMat1Handle, page, addr) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (result2 != NULL && matGetRegister(&localRes2, sMat2Handle, page, addr) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (result3 != NULL && matGetRegister(&localRes3, sMat3Handle, page, addr) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (result4 != NULL && matGetRegister(&localRes4, sMat4Handle, page, addr) != ESP_OK)
    {
        return ESP_FAIL;
    }
    /* output results */
    if (result1 != NULL)
    {
        *result1 = localRes1;
    }
    if (result2 != NULL)
    {
        *result2 = localRes2;
    }
    if (result3 != NULL)
    {
        *result3 = localRes3;
    }
    if (result4 != NULL)
    {
        *result4 = localRes4;
    }
    return ESP_OK;
}

/**
 * Performs I2C transactions to move the matrix device to the desired
 * page and writes the provided data to the given register address.
 *
 * Parameters:
 *  - device: The I2C handle of the matrix IC to target.
 *  - page: The page the register exists in.
 *  - addr: The address of the register.
 *  - data: The data to write to the address.
 *
 * Returns: ESP_OK if successful. Otherwise, the page of the current
 *          device may have been changed.
 */
esp_err_t matSetRegister(i2c_master_dev_handle_t device, uint8_t page, uint8_t addr, uint8_t data)
{
    uint8_t buffer[2];
    /* gaurd input */
    if (page > 4 || device == NULL)
    {
        return ESP_FAIL;
    }
    /* move to the correct page */
    if (matSetPage(device, page) != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to set page");
        return ESP_FAIL;
    }
    /* transmit message */
    FILL_BUFFER(addr, data);
    return i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS);
}

/**
 * Sets the target register for all matrix ICs.
 *
 * Parameters:
 *  - page: The page the register exists in.
 *  - addr: The address of the register.
 *  - data: The data to write to the address.
 *
 * Returns: ESP_OK if successful. Otherwise, the target
 *          register may have been changed in one or
 *          multiple matrices, but not all. Additionally,
 *          the page of each matrix may have been changed.
 */
esp_err_t matSetRegisters(uint8_t page, uint8_t addr, uint8_t data)
{
    /* guard input */
    if (page > 4)
    {
        return ESP_FAIL;
    }
    /* set registers */
    if (matSetRegister(sMat1Handle, page, addr, data) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (matSetRegister(sMat2Handle, page, addr, data) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (matSetRegister(sMat3Handle, page, addr, data) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (matSetRegister(sMat4Handle, page, addr, data) != ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * Performs I2C transactions to set the configuration
 * register of each of the matrix ICs to the provided
 * values.
 *
 * Parameters:
 *  - page: The page of the target register.
 *  - addr: The address of the target register.
 *  - mat(1/2/3)Cfg: The register value for matrix (1/2/3).
 *
 * Returns: ESP_OK if successful. Otherwise, the
 *          configuration of each matrix may have
 *          been changed, but not all.
 */
esp_err_t matSetRegistersSeparate(uint8_t page, uint8_t addr, uint8_t mat1val, uint8_t mat2val, uint8_t mat3val, uint8_t mat4val)
{
    /* change register values */
    if (matSetRegister(sMat1Handle, page, addr, mat1val) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (matSetRegister(sMat2Handle, page, addr, mat2val) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (matSetRegister(sMat3Handle, page, addr, mat3val) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (matSetRegister(sMat4Handle, page, addr, mat4val) != ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * into the provided operation mode.
 *
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t matSetOperatingMode(enum Operation setting)
{
    esp_err_t err;
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg, mat4Cfg;
    /* Read current configuration states */
    if (matGetRegisters(&mat1Cfg, &mat2Cfg, &mat3Cfg, &mat4Cfg, CONFIG_PAGE, CONFIG_REG_ADDR) != ESP_OK)
    {
        return ESP_FAIL;
    }
    /* Generate and set new configuration states */
    matSetBits(&mat1Cfg, SOFTWARE_SHUTDOWN_BITS, (uint8_t) setting);
    matSetBits(&mat2Cfg, SOFTWARE_SHUTDOWN_BITS, (uint8_t) setting);
    matSetBits(&mat3Cfg, SOFTWARE_SHUTDOWN_BITS, (uint8_t) setting);
    matSetBits(&mat4Cfg, SOFTWARE_SHUTDOWN_BITS, (uint8_t) setting);
    err = matSetRegistersSeparate(CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg, mat2Cfg, mat3Cfg, mat4Cfg);
    return err;
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * into the provided detection mode.
 *
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t matSetOpenShortDetection(enum ShortDetectionEnable setting)
{
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg, mat4Cfg;
    /* Read current configuration states */
    if (matGetRegisters(&mat1Cfg, &mat2Cfg, &mat3Cfg, &mat4Cfg, CONFIG_PAGE, CONFIG_REG_ADDR) != ESP_OK)
    {
        return ESP_FAIL;
    }
    /* Generate and set new configuration states */
    matSetBits(&mat1Cfg, OPEN_SHORT_DETECT_EN_BITS, (uint8_t)setting);
    matSetBits(&mat2Cfg, OPEN_SHORT_DETECT_EN_BITS, (uint8_t)setting);
    matSetBits(&mat3Cfg, OPEN_SHORT_DETECT_EN_BITS, (uint8_t)setting);
    matSetBits(&mat4Cfg, OPEN_SHORT_DETECT_EN_BITS, (uint8_t)setting);
    return matSetRegistersSeparate(CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg, mat2Cfg, mat3Cfg, mat4Cfg);
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * into the provided logic level.
 *
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t matSetLogicLevel(enum LogicLevel setting)
{
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg, mat4Cfg;
    /* Read current configuration states */
    if (matGetRegisters(&mat1Cfg, &mat2Cfg, &mat3Cfg, &mat4Cfg, CONFIG_PAGE, CONFIG_REG_ADDR) != ESP_OK)
    {
        return ESP_FAIL;
    }
    /* Generate and set new configuration states */
    matSetBits(&mat1Cfg, LOGIC_LEVEL_CNTRL_BITS, (uint8_t)setting);
    matSetBits(&mat2Cfg, LOGIC_LEVEL_CNTRL_BITS, (uint8_t)setting);
    matSetBits(&mat3Cfg, LOGIC_LEVEL_CNTRL_BITS, (uint8_t)setting);
    matSetBits(&mat4Cfg, LOGIC_LEVEL_CNTRL_BITS, (uint8_t)setting);
    return matSetRegistersSeparate(CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg, mat2Cfg, mat3Cfg, mat4Cfg);
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * int othe provided SWx setting.
 *
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t matSetSWxSetting(enum SWXSetting setting)
{
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg, mat4Cfg;
    /* Read current configuration states */
    if (matGetRegisters(&mat1Cfg, &mat2Cfg, &mat3Cfg, &mat4Cfg, CONFIG_PAGE, CONFIG_REG_ADDR) != ESP_OK)
    {
        return ESP_FAIL;
    }
    /* Generate and set new configuration states */
    matSetBits(&mat1Cfg, SWX_SETTING_BITS, (uint8_t)setting);
    matSetBits(&mat2Cfg, SWX_SETTING_BITS, (uint8_t)setting);
    matSetBits(&mat3Cfg, SWX_SETTING_BITS, (uint8_t)setting);
    matSetBits(&mat4Cfg, SWX_SETTING_BITS, (uint8_t)setting);
    return matSetRegistersSeparate(CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg, mat2Cfg, mat3Cfg, mat4Cfg);
}

/**
 * Performs I2C transactions to change the global current
 * control setting of each matrix to the provided value.
 *
 * Returns: ESP_OK if successful. Otherwise, the register value
 *          of each matrix may have been changed, but not all.
 */
esp_err_t matSetGlobalCurrentControl(uint8_t value)
{
    return matSetRegisters(CONFIG_PAGE, CURRENT_CNTRL_REG_ADDR, (uint8_t) value);
}

/**
 * Performs I2C transactions to change the resistor pullup
 * value of each matrix to the provided value.
 *
 * Returns: ESP_OK if successful. Otherwise, the register value
 *          of each matrix may have been changed, but not all.
 */
esp_err_t matSetResistorPullupSetting(enum ResistorSetting setting)
{
    uint8_t mat1Reg, mat2Reg, mat3Reg, mat4Reg;
    /* Read current resistor states */
    if (matGetRegisters(&mat1Reg, &mat2Reg, &mat3Reg, &mat4Reg, CONFIG_PAGE, PULL_SEL_REG_ADDR) != ESP_OK)
    {
        return ESP_FAIL;
    }
    /* Generate and set new resistor states */
    matSetBits(&mat1Reg, PUR_BITS, (uint8_t) setting);
    matSetBits(&mat2Reg, PUR_BITS, (uint8_t) setting);
    matSetBits(&mat3Reg, PUR_BITS, (uint8_t) setting);
    matSetBits(&mat4Reg, PUR_BITS, (uint8_t) setting);
    return matSetRegistersSeparate(CONFIG_PAGE, PULL_SEL_REG_ADDR, mat1Reg, mat2Reg, mat3Reg, mat4Reg);
}

/**
 * Performs I2C transactions to change the resistor pulldown
 * value of each matrix to the provided value.
 *
 * Returns: ESP_OK if successful. Otherwise, the register value
 *          of each matrix may have been changed, but not all.
 */
esp_err_t matSetResistorPulldownSetting(enum ResistorSetting setting)
{
    uint8_t mat1Reg, mat2Reg, mat3Reg, mat4Reg;
    /* Read current resistor states */
    if (matGetRegisters(&mat1Reg, &mat2Reg, &mat3Reg, &mat4Reg, CONFIG_PAGE, PULL_SEL_REG_ADDR) != ESP_OK)
    {
        return ESP_FAIL;
    }
    /* Generate and set new resistor states */
    matSetBits(&mat1Reg, PDR_BITS, (uint8_t)setting);
    matSetBits(&mat2Reg, PDR_BITS, (uint8_t)setting);
    matSetBits(&mat3Reg, PDR_BITS, (uint8_t)setting);
    matSetBits(&mat4Reg, PDR_BITS, (uint8_t)setting);
    return matSetRegistersSeparate(CONFIG_PAGE, PULL_SEL_REG_ADDR, mat1Reg, mat2Reg, mat3Reg, mat4Reg);
}

/**
 * Sets the PWM frequency of all matrix ICs.
 */
esp_err_t matSetPWMFrequency(enum PWMFrequency freq)
{
    return matSetRegisters(CONFIG_PAGE, PWM_FREQ_REG_ADDR, (uint8_t)freq);
}

/**
 * Resets all matrix registers to default values.
 *
 * Returns: ESP_OK if successful. Otherwise, some of
 *          the matrices may have been reset, but not all.
 */
esp_err_t matReset(void)
{
    return matSetRegisters(CONFIG_PAGE, RESET_REG_ADDR, RESET_KEY);
}

/**
 * Sets the color of the LED corresponding to Kicad hardware
 * number ledNum. Internally, this changes the PWM duty in
 * 256 steps.
 */
esp_err_t matSetColor(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t err;
    LEDReg ledReg;
    i2c_master_dev_handle_t matrixHandle;
    uint8_t page;
    /* guard input */
    if (ledNum == 0 || ledNum > MAX_NUM_LEDS_REG)
    {
        return ESP_FAIL;
    }
    /* determine the correct PWM registers */
    ledReg = LEDNumToReg[ledNum];
    err = matParseLEDRegisterInfo(&matrixHandle, &page, NULL, ledReg);
    if (err != ESP_OK)
    {
        return err;
    }
    /* set PWM registers to provided values */
    err = matSetRegister(matrixHandle, page, ledReg.red, red);
    if (err != ESP_OK)
    {
        return err; 
    }
    err = matSetRegister( matrixHandle, page, ledReg.green, green);
    if (err != ESP_OK)
    {
        return err;
    }
    err = matSetRegister(matrixHandle, page, ledReg.blue, blue);
    if (err != ESP_OK)
    {
        return err;
    }
    return ESP_OK;
}

/**
 * Controls the DC output current of the LED corresponding
 * to Kicad hardware number ledNum. See pg. 13 of the datasheet
 * for exact calculations. This can be considered a dimming
 * function.
 */
esp_err_t matSetScaling(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t err;
    LEDReg ledReg;
    i2c_master_dev_handle_t matrixHandle;
    uint8_t page;
    /* guard input */
    if (ledNum == 0 || ledNum > MAX_NUM_LEDS_REG)
    {
        return ESP_FAIL;
    }
    /* determine the correct PWM registers */
    ledReg = LEDNumToReg[ledNum];
    err = matParseLEDRegisterInfo(&matrixHandle, NULL, &page, ledReg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to parse register info. ledReg: %d", ledReg.matrix);
        return err;
    }
    /* set PWM registers to provided values */
    err = matSetRegister(matrixHandle, page, ledReg.red, red);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to set red register");
        return err;
    }
    err = matSetRegister(matrixHandle, page, ledReg.green, green);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to set green register");
        return err;
    }
    err = matSetRegister(matrixHandle, page, ledReg.blue, blue);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to set blue register");
        return err;
    }
    return ESP_OK;
}

#if CONFIG_DISABLE_TESTING_FEATURES == false // this is inverted for the esp-idf vscode extension
/*******************************************/
/*            TESTING FEATURES             */
/*******************************************/

esp_err_t matReleaseBus1(void) {
    esp_err_t ret;
    /* input guards */
    if (sI2CBus1 == NULL) {
        return ESP_OK;
    }
    ret = i2c_master_bus_rm_device(sMat1Handle);
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }
    ret = i2c_master_bus_rm_device(sMat2Handle);
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }
    ret = i2c_del_master_bus(sI2CBus1);
    if (ret == ESP_OK) {
        /* remove dangling pointer */
        sI2CBus1 = NULL;
        sMat1Handle = NULL;
        sMat2Handle = NULL;
    }
    return ret;
}

esp_err_t matReleaseBus2(void) {
    esp_err_t ret;
    /* input guards */
    if (sI2CBus2 == NULL) {
        return ESP_OK;
    }
    ret = i2c_master_bus_rm_device(sMat3Handle);
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }
    ret = i2c_master_bus_rm_device(sMat4Handle);
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }
    ret = i2c_del_master_bus(sI2CBus2);
    if (ret == ESP_OK) {
        /* remove dangling pointer */
        sI2CBus2 = NULL;
        sMat3Handle = NULL;
        sMat4Handle = NULL;
    }
    return ret;
}
#endif /* CONFIG_DISABLE_TESTING_FEATURES == false */

#endif /* CONFIG_HARDWARE_VERSION == 2 */