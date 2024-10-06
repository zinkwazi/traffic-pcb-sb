/**
 * dots_matrix.c
 * 
 * This file contains a hardware abstraction layer
 * for the dots matrix led driver ICs. The esp32
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

#define TAG "DotsMatrix"

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

static inline void resetStaticVars(void) {
    currState.mat1 = 0;
    currState.mat2 = 0;
    currState.mat3 = 0;
    I2CQueue = NULL;
    master_bus = NULL;
    matrix1_handle = NULL;
    matrix2_handle = NULL;
    matrix3_handle = NULL;
}

/**
 * This task manages interaction with the I2C peripheral,
 * which should be interacted with only through the functions
 * below. These functions abstract queueing interaction with 
 * the dots matrices.
 */
void vI2CGatekeeperTask(void *pvParameters) {
    struct I2CGatekeeperTaskParameters *params = (struct I2CGatekeeperTaskParameters *) pvParameters;
    I2CCommand command;
    esp_err_t err;
    /* One time setup */
    resetStaticVars();
    err = dotsInitializeBus();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize I2C bus");
    }
    err = dotsAssertConnected();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not find dots matrices on I2C bus... retrying...");
    }
    while (err != ESP_OK) {
        err = dotsAssertConnected();
    }
    /* Wait for commands and execute them forever */
    for (;;) {  // This task should never end
        if (pdPASS != xQueueReceive(params->I2CQueueHandle, &command, INT_MAX)) {
            ESP_LOGD(TAG, "I2C Gatekeeper timed out while waiting for command on queue");
            continue;
        }
        executeI2CCommand(&command);
    }
    ESP_LOGE(TAG, "I2C gatekeeper task is exiting! This should be impossible!");
    vTaskDelete(NULL); // exit safely (should never happen)
}

/**
 * This function maps the I2CCommandFunc enum to actual functions
 * and executes them, performing error callbacks when necessary.
 */
void executeI2CCommand(I2CCommand *command) {
    esp_err_t err = ESP_OK;
    switch (command->func) {
        case INITIALIZE_BUS:
            err = dotsInitializeBus();
            break;
        case ASSERT_CONNECTED:
            err = dotsAssertConnected();
            break;
        case SET_OPERATING_MODE:
            err = dotsSetOperatingMode(*((enum Operation*) command->params));
            break;
        case SET_OPEN_SHORT_DETECTION:
            err = dotsSetOpenShortDetection(*((enum ShortDetectionEnable*) command->params));
            break;
        case SET_LOGIC_LEVEL:
            err = dotsSetLogicLevel(*((enum LogicLevel*) command->params));
            break;
        case SET_SWX_SETTING:
            err = dotsSetSWxSetting(*((enum SWXSetting*) command->params));
            break;
        case SET_GLOBAL_CURRENT_CONTROL:
            err = dotsSetGlobalCurrentControl(*((uint8_t*) command->params));
            break;
        case SET_RESISTOR_PULLUP:
            err = dotsSetResistorPullupSetting(*((enum ResistorSetting*) command->params));
            break;
        case SET_RESISTOR_PULLDOWN:
            err = dotsSetResistorPulldownSetting(*((enum ResistorSetting*) command->params));
            break;
        case SET_PWM_FREQUENCY:
            err = dotsSetPWMFrequency(*((enum PWMFrequency*) command->params));
            break;
        default:
            ESP_LOGW(TAG, "I2C gatekeeper recieved an invalid command");
            return;
    }
    if (err != ESP_OK) {
        if (command->errCallback != NULL) {
            command->errCallback(err); // TODO: replace this with a more secure error handling method
        } else {
            ESP_LOGE(TAG, "I2C gatekeeper recieved a null error callback function!");
        }
    }
}

esp_err_t dotsInitializeBus(void) {
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

esp_err_t dotsAssertConnected(void) {
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
 *  - value: The value to change bitMask bits in prev to. If
 *           greater than what bitMask can contain, this value
 *           will silently be shortened.
 */
inline void dotsSetBits(uint8_t *reg, uint8_t bitMask, uint8_t value) {
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
esp_err_t dotsSetPage(i2c_master_dev_handle_t device, uint8_t page) {
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
 * page and read the IC response of requesting data at the address.
 * 
 * Parameters:
 *  - result: The location to store the result of the register read.
 *  - device: The i2c handle of the matrix IC.
 *  - page: The page the register exists in.
 *  - addr: The address of the register to query.
 */
esp_err_t dotsGetRegister(uint8_t *result, i2c_master_dev_handle_t device, uint8_t page, uint8_t addr) {
    /* input guards */
    ESP_RETURN_ON_FALSE(
        (result != NULL), ESP_FAIL,
        TAG, "encountered unexpected NULL function parameter"
    );
    /* Set page and read config registers */
    dotsSetPage(device, page);
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit_receive(device, &addr, 1, result, 1, I2C_TIMEOUT_MS),
        TAG, "failed to read matrix register"
    );
    return ESP_OK;
}

/**
 * Performs I2C transactions to move matrix IC pages to the desired 
 * page and read the IC response of requesting data at the address.
 * 
 * Performs I2C transactions to move the matrix device to the desired
 * page and writes the provided data to the given register address.
 */
esp_err_t dotsSetRegister(i2c_master_dev_handle_t device, uint8_t page, uint8_t addr, uint8_t data) {
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
            dotsSetPage(device, page),
            TAG, "failed to change matrix 1 page"
        );
        currState.mat1 = page;
    } else if (device == matrix2_handle && page != currState.mat2) {
        ESP_RETURN_ON_ERROR(
            dotsSetPage(device, page),
            TAG, "failed to change matrix 2 page"
        );
        currState.mat2 = page;
    } else if (device == matrix3_handle && page != currState.mat3) {
        ESP_RETURN_ON_ERROR(
            dotsSetPage(device, page),
            TAG, "failed to change matrix 3 page"
        );
        currState.mat3 = page;
    }
    /* transmit message */
    FILL_BUFFER(addr, data);
    return i2c_master_transmit(device, buffer, 2, I2C_TIMEOUT_MS);
}

/**
 * Performs I2C transactions that put each of the matrix ICs
 * into the provided operation mode.
 */
esp_err_t dotsSetOperatingMode(enum Operation setting) {
    /* Read current configuration states */
    uint8_t mat1Cfg, mat2Cfg, mat3Cfg;
    ESP_RETURN_ON_ERROR(
        dotsGetRegister(&mat1Cfg, matrix1_handle, CONFIG_PAGE, CONFIG_REG_ADDR),
        TAG, "failed to retrieve current matrix 1 configuration"
    );
    ESP_RETURN_ON_ERROR(
        dotsGetRegister(&mat2Cfg, matrix2_handle, CONFIG_PAGE, CONFIG_REG_ADDR),
        TAG, "failed to retrieve current matrix 2 configuration"
    );
    ESP_RETURN_ON_ERROR(
        dotsGetRegister(&mat2Cfg, matrix2_handle, CONFIG_PAGE, CONFIG_REG_ADDR),
        TAG, "failed to retrieve current matrix 2 configuration"
    );
    /* Generate new configuration states */
    dotsSetBits(&mat1Cfg, SOFTWARE_SHUTDOWN_BITS, setting);
    dotsSetBits(&mat2Cfg, SOFTWARE_SHUTDOWN_BITS, setting);
    dotsSetBits(&mat3Cfg, SOFTWARE_SHUTDOWN_BITS, setting);
    /* Send new config states to matrices */
    ESP_RETURN_ON_ERROR(
        dotsSetRegister(matrix1_handle, CONFIG_PAGE, CONFIG_REG_ADDR, mat1Cfg),
        TAG, "failed to change matrix 1 config register"
    );
    ESP_RETURN_ON_ERROR(
        dotsSetRegister(matrix2_handle, CONFIG_PAGE, CONFIG_REG_ADDR, mat2Cfg),
        TAG, "failed to change matrix 2 config register"
    );
    ESP_RETURN_ON_ERROR(
        dotsSetRegister(matrix3_handle, CONFIG_PAGE, CONFIG_REG_ADDR, mat3Cfg),
        TAG, "failed to change matrix 3 config register"
    );
    return ESP_OK;
}

esp_err_t dotsSetOpenShortDetection(enum ShortDetectionEnable setting) {
    // TODO: Implement Function
    return ESP_FAIL;
}

esp_err_t dotsSetLogicLevel(enum LogicLevel setting) {
    // TODO: Implement Function
    return ESP_FAIL;
}

esp_err_t dotsSetSWxSetting(enum SWXSetting setting) {
    // TODO: Implement Function
    return ESP_FAIL;
}

esp_err_t dotsSetGlobalCurrentControl(uint8_t value) {
    // TODO: Implement Function
    return ESP_FAIL;
}

esp_err_t dotsSetResistorPullupSetting(enum ResistorSetting setting) {
    // TODO: Implement Function
    return ESP_OK;
}

esp_err_t dotsSetResistorPulldownSetting(enum ResistorSetting setting) {
    // TODO: Implement Function
    return ESP_FAIL;
}

esp_err_t dotsSetPWMFrequency(enum PWMFrequency freq) {
    ESP_RETURN_ON_ERROR(
        dotsSetRegister(matrix1_handle, 4, PWM_FREQ_REG_ADDR, (uint8_t) freq),
        TAG, "dotsChangePWMFrequency failed to change PWM frequency of matrix 1"
    );
    ESP_RETURN_ON_ERROR(
        dotsSetRegister(matrix2_handle, 4, PWM_FREQ_REG_ADDR, (uint8_t) freq),
        TAG, "dotsChangePWMFrequency failed to change PWM frequency of matrix 2"
    );
    ESP_RETURN_ON_ERROR(
        dotsSetRegister(matrix3_handle, 4, PWM_FREQ_REG_ADDR, (uint8_t) freq),
        TAG, "dotsChangePWMFrequency failed to change PWM frequency of matrix 3"
    );
    return ESP_OK;
}

/**
 * Resets all matrix registers to default values.
 */
esp_err_t dotsReset(void) {
    // TODO: Implement Function
    return ESP_FAIL;
}