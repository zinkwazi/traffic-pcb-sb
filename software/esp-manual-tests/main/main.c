#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
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
    static struct timeval prevTime = {
        .tv_sec = 0,
        .tv_usec = 0,
    };
    struct timeval currTime;
    gettimeofday(&currTime, NULL);
    if ((currTime.tv_sec - prevTime.tv_sec) * 1000000 + (currTime.tv_usec - prevTime.tv_usec) < 250000)
    {
        return;
    } else
    {
        prevTime.tv_sec = currTime.tv_sec;
        prevTime.tv_usec = currTime.tv_usec;
    }
    SemaphoreHandle_t sema1 = (SemaphoreHandle_t) params;
    BaseType_t higherPrioTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sema1, &higherPrioTaskWoken);
    portYIELD_FROM_ISR(higherPrioTaskWoken);
}

void runLEDColorTest(void) {
    /* Initialize direction button */
    SemaphoreHandle_t sema = xSemaphoreCreateBinary();
    gpio_install_isr_service(0);
    gpio_set_direction(T_SW_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(T_SW_PIN, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(T_SW_PIN, buttonISR, sema);
    dotsResetStaticVars();
    dInitializeBus(I2C_PORT, SDA_PIN, SCL_PIN);
    dAssertConnected();
    dReset();
    dSetGlobalCurrentControl(GLOBAL_CURRENT);
    dSetOperatingMode(NORMAL_OPERATION);
    for (int i = 1; i < MAX_NUM_LEDS; i++) {
        LEDReg reg = LEDNumToReg[i];
        dSetScaling(i, 0xFF, 0xFF, 0xFF);

        dSetColor(i, 0xFF, 0x00, 0x00);
        ESP_LOGI(TAG, "LED %d RED  , 0x%X", i, reg.red);
        gpio_intr_enable(T_SW_PIN);
        while (xSemaphoreTake(sema, INT_MAX) != pdTRUE) {}
        gpio_intr_disable(T_SW_PIN);
        
        dSetColor(i, 0x00, 0xFF, 0x00);
        ESP_LOGI(TAG, "LED %d GREEN, 0x%X", i, reg.green);
        gpio_intr_enable(T_SW_PIN);
        while (xSemaphoreTake(sema, INT_MAX) != pdTRUE) {}
        gpio_intr_disable(T_SW_PIN);

        dSetColor(i, 0x00, 0x00, 0xFF);
        ESP_LOGI(TAG, "LED %d BLUE , 0x%X", i, reg.blue);
        gpio_intr_enable(T_SW_PIN);
        while (xSemaphoreTake(sema, INT_MAX) != pdTRUE) {}
        gpio_intr_disable(T_SW_PIN);

        dSetColor(i, 0x00, 0x00, 0x00);
    }
}

void runPowerTest(void) {
    dotsResetStaticVars();
    dInitializeBus(I2C_PORT, SDA_PIN, SCL_PIN);
    dAssertConnected();
    dReset();
    dSetGlobalCurrentControl(0x80);
    dSetOperatingMode(NORMAL_OPERATION);
    for (int i = 1; i < MAX_NUM_LEDS; i++) {
        dSetScaling(i, 0xFF, 0xFF, 0xFF);
        dSetColor(i, 0xFF, 0xFF, 0xFF);
    }
}

void app_main(void)
{
#if CONFIG_RUN_LED_COLOR_TEST == true
    ESP_LOGI(TAG, "Running LED Color Test");
    runLEDColorTest();
#endif
#if CONFIG_RUN_POWER_TEST == true
    ESP_LOGI(TAG, "Running Power Test");
    runPowerTest();
#endif
    ESP_LOGI(TAG, "All tests complete.");
    for (;;) {
        vTaskDelay(INT_MAX);
    }
}