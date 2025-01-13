/**
 * test_led_registers.c
 * 
 * This file includes tests that verify LED register mapping
 * with the aid of a human verifier.
 */

#include <stdint.h>
#include "unity.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "led_registers.h"
#include "dots_matrix.h"


#define TAG "test"
#define MAX_NUM_LEDS sizeof(LEDNumToReg) / sizeof(LEDNumToReg[0])
/* The direction button is used as an 'OK' signal from the verifier. */
#define T_SW_PIN        GPIO_NUM_4

/* I2C Pins */
#define I2C_PORT        (-1) // auto-select available I2C port
#define SCL_PIN         GPIO_NUM_26
#define SDA_PIN         GPIO_NUM_27

#define GLOBAL_CURRENT 0x37

struct buttonParams {
    SemaphoreHandle_t sema1;
    SemaphoreHandle_t sema2;
};

typedef struct buttonParams buttonParams;

void buttonISR(void *params) {
    SemaphoreHandle_t sema1 = ((buttonParams *) params)->sema1;
    SemaphoreHandle_t sema2 = ((buttonParams *) params)->sema2;
    if (xSemaphoreTake(sema2, 0) != pdTRUE) {
        return;
    }
    BaseType_t higherPrioTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sema1, &higherPrioTaskWoken);
    portYIELD_FROM_ISR(higherPrioTaskWoken);
}

TEST_CASE("LEDRegisters", "[DotsMatrix]")
{
    /* Initialize direction button */
    SemaphoreHandle_t sema = xSemaphoreCreateBinary();
    SemaphoreHandle_t sema2 = xSemaphoreCreateBinary();
    static buttonParams params;
    params.sema1 = sema;
    params.sema2 = sema2;
    TEST_ASSERT_NOT_EQUAL(NULL, sema);
    TEST_ASSERT_EQUAL(ESP_OK, gpio_install_isr_service(0));
    TEST_ASSERT_EQUAL(ESP_OK, gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT));
    TEST_ASSERT_EQUAL(ESP_OK, gpio_set_intr_type(T_SW_PIN, GPIO_INTR_NEGEDGE));
    TEST_ASSERT_EQUAL(ESP_OK, gpio_isr_handler_add(T_SW_PIN, buttonISR, &params));
    dotsResetStaticVars();
    TEST_ASSERT_EQUAL(ESP_OK, dInitializeBus(I2C_PORT, SDA_PIN, SCL_PIN));
    TEST_ASSERT_EQUAL(ESP_OK, dAssertConnected());
    TEST_ASSERT_EQUAL(ESP_OK, dReset());
    TEST_ASSERT_EQUAL(ESP_OK, dSetGlobalCurrentControl(GLOBAL_CURRENT));
    TEST_ASSERT_EQUAL(ESP_OK, dSetOperatingMode(NORMAL_OPERATION));
    for (int i = 1; i < MAX_NUM_LEDS; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, dSetScaling(i, 0xFF, 0xFF, 0xFF));
        LEDReg reg = LEDNumToReg[i];
        TEST_ASSERT_EQUAL(ESP_OK, dSetColor(i, 0xFF, 0x00, 0x00));
        
        ESP_LOGI(TAG, "LED %d RED  , 0x%X", i, reg.red);
        xSemaphoreGive(sema2);
        TEST_ASSERT_EQUAL(ESP_OK, gpio_intr_enable(T_SW_PIN));
        while (xSemaphoreTake(sema, INT_MAX) != pdTRUE) {}
        TEST_ASSERT_EQUAL(ESP_OK, gpio_intr_disable(T_SW_PIN));
        

        TEST_ASSERT_EQUAL(ESP_OK, dSetColor(i, 0x00, 0xFF, 0x00));
        ESP_LOGI(TAG, "LED %d GREEN, 0x%X", i, reg.green);
        xSemaphoreGive(sema2);
        TEST_ASSERT_EQUAL(ESP_OK, gpio_intr_enable(T_SW_PIN));
        while (xSemaphoreTake(sema, INT_MAX) != pdTRUE) {}
        TEST_ASSERT_EQUAL(ESP_OK, gpio_intr_disable(T_SW_PIN));

        TEST_ASSERT_EQUAL(ESP_OK, dSetColor(i, 0x00, 0x00, 0xFF));
        ESP_LOGI(TAG, "LED %d BLUE , 0x%X", i, reg.blue);
        xSemaphoreGive(sema2);
        TEST_ASSERT_EQUAL(ESP_OK, gpio_intr_enable(T_SW_PIN));
        while (xSemaphoreTake(sema, INT_MAX) != pdTRUE) {}
        TEST_ASSERT_EQUAL(ESP_OK, gpio_intr_disable(T_SW_PIN));

        TEST_ASSERT_EQUAL(ESP_OK, dSetColor(i, 0x00, 0x00, 0x00));
    }
}