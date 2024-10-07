/**
 * d_matrix.c
 * 
 * This file contains a hardware abstraction layer
 * for the d matrix led driver ICs. The esp32
 * interacts with these ICs through I2C.
 * 
 * See: https://www.lumissil.com/assets/pdf/core/IS31FL3741A_DS.pdf.
 */

#include "dots_matrix.h"

/* IDF component includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2c_master.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_check.h"

/* Component includes */
#include "pinout.h"
#include "led_registers.h"

#define TAG "dots_matrix"

#define I2C_TIMEOUT_MS (100)

#define MAT_UPPER_ADDR  0b011000
#define MAT1_LOWER_ADDR 0x00
#define MAT2_LOWER_ADDR 0x11
#define MAT3_LOWER_ADDR 0x10
#define MAT1_ADDR (MAT_UPPER_ADDR | MAT1_LOWER_ADDR)
#define MAT2_ADDR (MAT_UPPER_ADDR | MAT2_LOWER_ADDR)
#define MAT3_ADDR (MAT_UPPER_ADDR | MAT3_LOWER_ADDR)

#define BUS_SPEED_HZ  400000 // 400kHz maximum
#define SCL_WAIT_US   0  // use default value
#define PROBE_WAIT_MS 100

/* Matrix Driver IC High Level Registers */
#define CMD_REG_ADDR                0xFD
#define CMD_REG_WRITE_LOCK_ADDR     0xFE
#define CMD_REG_WRITE_KEY           0b11000101
#define INTR_MSK_REG_ADDR           0xF0
#define INTR_STAT_REG_ADDR          0xF1
#define ID_REG_ADDR                 0xFC

/* Matrix Driver IC Pages */
#define PWM1_PAGE                   0
#define PWM2_PAGE                   1
#define SCALING1_PAGE               2
#define SCALING2_PAGE               3
#define CONFIG_PAGE                 4

/* Matrix Driver IC Function Registers */
#define CONFIG_REG_ADDR             0x00
#define CURRENT_CNTRL_REG_ADDR      0x01
#define PULL_SEL_REG_ADDR           0x02
#define PWM_FREQ_REG_ADDR           0x36
#define RESET_REG_ADDR              0x3F

/* Configuration Register Bits */
#define SOFTWARE_SHUTDOWN_BITS      0x01
#define OPEN_SHORT_DETECT_EN_BITS   0x06
#define LOGIC_LEVEL_CNTRL_BITS      0x08
#define SWX_SETTING_BITS            0xF0

/* Pull Up/Down Register Bits */
#define PUR_BITS        0x07
#define PDR_BITS        0x70

/* PWM Frequency Setting Register Bits */
#define PWS_BITS        0x0F

/* Reset Register */
#define RESET_KEY       0xAE

#define FILL_BUFFER(reg, data) buffer[0] = reg; buffer[1] = data;

esp_err_t dInitializeBus(void) {
    i2c_master_bus_config_t master_bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT, // not sure about this
        .glitch_ignore_cnt = 7, // typical value
        .intr_priority = 0, // may be one of level 1, 2, or 3 when set to 0
        .flags.enable_internal_pullup = false,
    };
    i2c_device_config_t matrix_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MAT1_ADDR,
        .scl_speed_hz = BUS_SPEED_HZ,
        .scl_wait_us = SCL_WAIT_US, // use default value
    };
    /* Initialize I2C bus */
    ESP_RETURN_ON_ERROR(
        i2c_new_master_bus(&master_bus_config, &master_bus),
        TAG, "failed to initialize new i2c master bus struct"
    );
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(master_bus, &matrix_config, &matrix1_handle),
        TAG, "failed to add matrix1 device to i2c bus"
    );
    matrix_config.device_address = MAT2_ADDR;
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(master_bus, &matrix_config, &matrix2_handle),
        TAG, "failed to add matrix2 device to i2c bus"
    );
    matrix_config.device_address = MAT3_ADDR;
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(master_bus, &matrix_config, &matrix3_handle),
        TAG, "failed to add matrix3 device to i2c bus"
    );
    return ESP_OK;
}

esp_err_t dAssertConnected(void) {
    ESP_RETURN_ON_ERROR(
        i2c_master_probe(master_bus, MAT1_ADDR, PROBE_WAIT_MS),
        TAG, "failed to detect matrix1 on i2c bus"
    );
    ESP_RETURN_ON_ERROR(
        i2c_master_probe(master_bus, MAT2_ADDR, PROBE_WAIT_MS),
        TAG, "failed to detect matrix2 on i2c bus"
    );
    ESP_RETURN_ON_ERROR(
        i2c_master_probe(master_bus, MAT2_ADDR, PROBE_WAIT_MS),
        TAG, "failed to detect matrix2 on i2c bus"
    );
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
void dSetBits(uint8_t *reg, uint8_t bitMask, uint8_t value) {
    /* Align value to bitMask */
    for (uint8_t currShift = 0; currShift < 8; currShift++) {
        if (bitMask % (0x01 << currShift) != 0x00) {
            value <<= currShift;
            break;
        }
    }
    /* Update prev mask bits */
    *reg &= ~bitMask; // clear previous mask bits
    *reg |= (bitMask & value); // set value to mask bits of reg
}

/**
 * Changes the current matrix device register page by unlocking
 * and writing to the command register via I2C. 
 * 
 * If the saved state in the ESP32 (currState) indicates that 
 * the device is already in the requested page, the function 
 * will return ESP_OK without performing any I2C transactions.
 */
esp_err_t dSetPage(i2c_master_dev_handle_t device, uint8_t page) {
    uint8_t buffer[2];
    /* input guards */
    ESP_RETURN_ON_FALSE(
        (page <= 4), ESP_FAIL,
        TAG, "encountered invalid page number function parameter"
    );
    ESP_RETURN_ON_FALSE(
        (device != NULL), ESP_FAIL,
        TAG, "encountered NULL i2c device handle function parameter"
    );
    /* check current page setting */
    if (device == matrix1_handle && page == currState.mat1) {
        return ESP_OK;
    }
    if (device == matrix2_handle && page == currState.mat2) {
        return ESP_OK;
    }
    if (device == matrix3_handle && page == currState.mat3) {
        return ESP_OK;
    }
    /* unlock command register */
    FILL_BUFFER(CMD_REG_WRITE_LOCK_ADDR, CMD_REG_WRITE_KEY);
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS),
        TAG, "failed to unlock command register"
    );
    /* update page */
    FILL_BUFFER(CMD_REG_ADDR, page);
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS),
        TAG, "failed to transmit change page i2c transaction"
    );
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
esp_err_t dGetRegister(uint8_t *result, i2c_master_dev_handle_t device, uint8_t page, uint8_t addr) {
    /* input guards */
    ESP_RETURN_ON_FALSE(
        (result != NULL), ESP_FAIL,
        TAG, "encountered unexpected NULL function parameter"
    );
    /* Set page and read config registers */
    dSetPage(device, page);
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit_receive(device, &addr, 1, result, 1, I2C_TIMEOUT_MS),
        TAG, "failed to read matrix register"
    );
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
 *  - page: The page the target register exists in.
 *  - addr: The address of the target register.
 * 
 * Returns: ESP_OK if successful. Otherwise, the data pointed to
 *          by the results are unmodified, however pages may
 *          have been modified.
 */
esp_err_t dGetRegisters(uint8_t *result1, uint8_t *result2, uint8_t *result3, uint8_t page, uint8_t addr) {
    uint8_t localRes1, localRes2, localRes3; // if something fails, do not modify results
    /* guard input */
    ESP_RETURN_ON_FALSE(
        (page <= 4), ESP_FAIL,
        TAG, "encountered invalid page number function parameter"
    );
    /* get registers */
    if (result1 != NULL) {
        ESP_RETURN_ON_ERROR(
            dGetRegister(&localRes1, matrix1_handle, page, addr),
            TAG, "failed to read matrix 1 register"
        );
    }
    if (result2 != NULL) {
        ESP_RETURN_ON_ERROR(
            dGetRegister(&localRes2, matrix2_handle, page, addr),
            TAG, "failed to read matrix 2 register"
        );
    }
    if (result3 != NULL) {
        ESP_RETURN_ON_ERROR(
            dGetRegister(&localRes3, matrix3_handle, page, addr),
            TAG, "failed to read matrix 3 register"
        );
    }
    /* output results */
    if (result1 != NULL) {
        *result1 = localRes1;
    }
    if (result2 != NULL) {
        *result2 = localRes2;
    }
    if (result3 != NULL) {
        *result3 = localRes3;
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
esp_err_t dSetRegister(i2c_master_dev_handle_t device, uint8_t page, uint8_t addr, uint8_t data) {
    uint8_t buffer[2];
    /* gaurd input */
    ESP_RETURN_ON_FALSE(
        (page <= 4), ESP_FAIL,
        TAG, "encountered invalid page number function parameter"
    );
    ESP_RETURN_ON_FALSE(
        (device != NULL), ESP_FAIL,
        TAG, "encountered NULL i2c device handle function parameter"
    );
    /* ensure that matrix is on the correct page */
    if (device == matrix1_handle && page != currState.mat1) {
        ESP_RETURN_ON_ERROR(
            dSetPage(device, page),
            TAG, "failed to change matrix 1 page"
        );
        currState.mat1 = page;
    } else if (device == matrix2_handle && page != currState.mat2) {
        ESP_RETURN_ON_ERROR(
            dSetPage(device, page),
            TAG, "failed to change matrix 2 page"
        );
        currState.mat2 = page;
    } else if (device == matrix3_handle && page != currState.mat3) {
        ESP_RETURN_ON_ERROR(
            dSetPage(device, page),
            TAG, "failed to change matrix 3 page"
        );
        currState.mat3 = page;
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
esp_err_t dSetRegisters(uint8_t page, uint8_t addr, uint8_t data) {
    /* guard input */
    ESP_RETURN_ON_FALSE(
        (page <= 4), ESP_FAIL,
        TAG, "encountered invalid page number function parameter"
    );
    /* set registers */
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix1_handle, page, addr, data),
        TAG, "could not set matrix 1 register"
    );
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix2_handle, page, addr, data),
        TAG, "could not set matrix 2 register"
    );
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix3_handle, page, addr, data),
        TAG, "could not set matrix 3 register"
    );
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
esp_err_t dSetRegistersSeparate(uint8_t page, uint8_t addr, uint8_t mat1val, uint8_t mat2val, uint8_t mat3val) {
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix1_handle, page, addr, mat1val),
        TAG, "failed to change matrix 1 config register"
    );
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix2_handle, page, addr, mat2val),
        TAG, "failed to change matrix 2 config register"
    );
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix3_handle, page, addr, mat3val),
        TAG, "failed to change matrix 3 config register"
    );
    return ESP_OK;
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * into the provided operation mode.
 * 
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dSetOperatingMode(enum Operation setting) {
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg;
    /* Read current configuration states */
    ESP_RETURN_ON_ERROR(
        dGetRegisters(&mat1Cfg, &mat2Cfg, &mat3Cfg, CONFIG_PAGE, CONFIG_REG_ADDR),
        TAG, "failed to retrieve current matrix configurations"
    );
    /* Generate and set new configuration states */
    dSetBits(&mat1Cfg, SOFTWARE_SHUTDOWN_BITS, (uint8_t) setting);
    dSetBits(&mat2Cfg, SOFTWARE_SHUTDOWN_BITS, (uint8_t) setting);
    dSetBits(&mat3Cfg, SOFTWARE_SHUTDOWN_BITS, (uint8_t) setting);
    return dSetRegistersSeparate(CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg, mat2Cfg, mat3Cfg);
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * into the provided detection mode.
 * 
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dSetOpenShortDetection(enum ShortDetectionEnable setting) {
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg;
    /* Read current configuration states */
    ESP_RETURN_ON_ERROR(
        dGetRegisters(&mat1Cfg, &mat2Cfg, &mat3Cfg, CONFIG_PAGE, CONFIG_REG_ADDR),
        TAG, "failed to retrieve current matrix configurations"
    );
    /* Generate and set new configuration states */
    dSetBits(&mat1Cfg, OPEN_SHORT_DETECT_EN_BITS, (uint8_t) setting);
    dSetBits(&mat2Cfg, OPEN_SHORT_DETECT_EN_BITS, (uint8_t) setting);
    dSetBits(&mat3Cfg, OPEN_SHORT_DETECT_EN_BITS, (uint8_t) setting);
    return dSetRegistersSeparate(CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg, mat2Cfg, mat3Cfg);
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * into the provided logic level.
 * 
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dSetLogicLevel(enum LogicLevel setting) {
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg;
    /* Read current configuration states */
    ESP_RETURN_ON_ERROR(
        dGetRegisters(&mat1Cfg, &mat2Cfg, &mat3Cfg, CONFIG_PAGE, CONFIG_REG_ADDR),
        TAG, "failed to retrieve current matrix configurations"
    );
    /* Generate and set new configuration states */
    dSetBits(&mat1Cfg, LOGIC_LEVEL_CNTRL_BITS, (uint8_t) setting);
    dSetBits(&mat2Cfg, LOGIC_LEVEL_CNTRL_BITS, (uint8_t) setting);
    dSetBits(&mat3Cfg, LOGIC_LEVEL_CNTRL_BITS, (uint8_t) setting);
    return dSetRegistersSeparate(CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg, mat2Cfg, mat3Cfg);
}

/**
 * Performs I2C transactions to put each of the matrix ICs
 * int othe provided SWx setting.
 * 
 * Returns: ESP_OK if successful. Otherwise, the configuration
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dSetSWxSetting(enum SWXSetting setting) {
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg;
    /* Read current configuration states */
    ESP_RETURN_ON_ERROR(
        dGetRegisters(&mat1Cfg, &mat2Cfg, &mat3Cfg, CONFIG_PAGE, CONFIG_REG_ADDR),
        TAG, "failed to retrieve current matrix configurations"
    );
    /* Generate and set new configuration states */
    dSetBits(&mat1Cfg, SWX_SETTING_BITS, (uint8_t) setting);
    dSetBits(&mat2Cfg, SWX_SETTING_BITS, (uint8_t) setting);
    dSetBits(&mat3Cfg, SWX_SETTING_BITS, (uint8_t) setting);
    return dSetRegistersSeparate(CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg, mat2Cfg, mat3Cfg);
}

/**
 * Performs I2C transactions to change the global current
 * control setting of each matrix to the provided value.
 * 
 * Returns: ESP_OK if successful. Otherwise, the register value
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dSetGlobalCurrentControl(uint8_t value) {
    ESP_RETURN_ON_ERROR(
        dSetRegisters(CONFIG_PAGE, CURRENT_CNTRL_REG_ADDR, (uint8_t) value),
        TAG, "failed to set matrix global current control registers"
    );
    return ESP_OK;
}

/**
 * Performs I2C transactions to change the resistor pullup
 * value of each matrix to the provided value.
 * 
 * Returns: ESP_OK if successful. Otherwise, the register value
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dSetResistorPullupSetting(enum ResistorSetting setting) {
    uint8_t mat1Reg, mat2Reg, mat3Reg;
    /* Read current resistor states */
    ESP_RETURN_ON_ERROR(
        dGetRegisters(&mat1Reg, &mat2Reg, &mat3Reg, CONFIG_PAGE, PULL_SEL_REG_ADDR),
        TAG, "failed to retrieve current matrix resistor states"
    );
    /* Generate and set new resistor states */
    dSetBits(&mat1Reg, PUR_BITS, (uint8_t) setting);
    dSetBits(&mat2Reg, PUR_BITS, (uint8_t) setting);
    dSetBits(&mat3Reg, PUR_BITS, (uint8_t) setting);
    return dSetRegistersSeparate(CONFIG_PAGE, PULL_SEL_REG_ADDR, mat1Reg, mat2Reg, mat3Reg);
}

/**
 * Performs I2C transactions to change the resistor pulldown
 * value of each matrix to the provided value.
 * 
 * Returns: ESP_OK if successful. Otherwise, the register value
 *          of each matrix may have been changed, but not all.
 */
esp_err_t dSetResistorPulldownSetting(enum ResistorSetting setting) {
    uint8_t mat1Reg, mat2Reg, mat3Reg;
    /* Read current resistor states */
    ESP_RETURN_ON_ERROR(
        dGetRegisters(&mat1Reg, &mat2Reg, &mat3Reg, CONFIG_PAGE, PULL_SEL_REG_ADDR),
        TAG, "failed to retrieve current matrix resistor states"
    );
    /* Generate and set new resistor states */
    dSetBits(&mat1Reg, PDR_BITS, (uint8_t) setting);
    dSetBits(&mat2Reg, PDR_BITS, (uint8_t) setting);
    dSetBits(&mat3Reg, PDR_BITS, (uint8_t) setting);
    return dSetRegistersSeparate(CONFIG_PAGE, PULL_SEL_REG_ADDR, mat1Reg, mat2Reg, mat3Reg);
}

/** 
 * Sets the PWM frequency of all matrix ICs. 
 */
esp_err_t dSetPWMFrequency(enum PWMFrequency freq) {
    ESP_RETURN_ON_ERROR(
        dSetRegisters(CONFIG_PAGE, PWM_FREQ_REG_ADDR, (uint8_t) freq),
        TAG, "failed to set PWM frequency registers"
    );
    return ESP_OK;
}

/**
 * Resets all matrix registers to default values.
 * 
 * Returns: ESP_OK if successful. Otherwise, some of
 *          the matrices may have been reset, but not all.
 */
esp_err_t dReset(void) {
    ESP_RETURN_ON_ERROR(
        dSetRegisters(CONFIG_PAGE, RESET_REG_ADDR, RESET_KEY),
        TAG, "failed to set reset registers to reset key"
    );
    return ESP_OK;
}

/**
 * Sets the color of the LED corresponding to Kicad hardware
 * number ledNum. Internally, this changes the PWM duty in
 * 256 steps.
 */
esp_err_t dSetColor(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue) {
    /* map led num 329 and 330 to reasonable numbers */ 
    ledNum = (ledNum == 329) ? 325 : ledNum;
    ledNum = (ledNum == 330) ? 326 : ledNum;
    /* guard input */
    ESP_RETURN_ON_FALSE(
        (ledNum > 0 && ledNum < 327), ESP_FAIL,
        TAG, "requested to set color for invalid led hardware number"
    );
    /* determine the correct PWM registers */
    LEDReg reg = LEDNumToReg[ledNum];
    i2c_master_dev_handle_t matrix_handle = NULL;
    if (ledNum >= 1 && ledNum <= 117) {
        matrix_handle = matrix1_handle;
    } else if (ledNum >= 118 && ledNum <= 234) {
        matrix_handle = matrix2_handle;
    } else if (ledNum >= 235 && ledNum <= 326) {
        matrix_handle = matrix3_handle;
    }
    ESP_RETURN_ON_FALSE(
        (matrix_handle != NULL), ESP_FAIL,
        TAG, "could not determine matrix handle for led hardware number"
    );
    /* set PWM registers to provided values */
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix_handle, reg.page, reg.red, red),
        TAG, "could not set red PWM value"
    );
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix_handle, reg.page, reg.green, green),
        TAG, "could not set green PWM value"
    );
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix_handle, reg.page, reg.blue, blue),
        TAG, "could not set blue PWM value"
    );
    return ESP_OK;
}

/**
 * Controls the DC output current of the LED corresponding
 * to Kicad hardware number ledNum. See pg. 13 of the datasheet
 * for exact calculations. This can be considered a dimming
 * function.
 */
esp_err_t dSetScaling(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue) {
    /* map led num 329 and 330 to reasonable numbers */ 
    ledNum = (ledNum == 329) ? 325 : ledNum;
    ledNum = (ledNum == 330) ? 326 : ledNum;
    /* guard input */
    ESP_RETURN_ON_FALSE(
        (ledNum > 0 && ledNum < 327), ESP_FAIL,
        TAG, "requested to set color for invalid led hardware number"
    );
    /* determine the correct PWM registers */
    LEDReg reg = LEDNumToReg[ledNum];
    i2c_master_dev_handle_t matrix_handle = NULL;
    if (ledNum >= 1 && ledNum <= 117) {
        matrix_handle = matrix1_handle;
    } else if (ledNum >= 118 && ledNum <= 234) {
        matrix_handle = matrix2_handle;
    } else if (ledNum >= 235 && ledNum <= 326) {
        matrix_handle = matrix3_handle;
    }
    ESP_RETURN_ON_FALSE(
        (matrix_handle != NULL), ESP_FAIL,
        TAG, "could not determine matrix handle for led hardware number"
    );
    /* set PWM registers to provided values */
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix_handle, reg.page + 2, reg.red, red),
        TAG, "could not set red PWM value"
    );
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix_handle, reg.page + 2, reg.green, green),
        TAG, "could not set green PWM value"
    );
    ESP_RETURN_ON_ERROR(
        dSetRegister(matrix_handle, reg.page + 2, reg.blue, blue),
        TAG, "could not set blue PWM value"
    );
    return ESP_OK;
}