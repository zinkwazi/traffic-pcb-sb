#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include "unity.h"
#include "verifier.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "led_registers.h"
#include "led_matrix.h"
#include "pinout.h"
#include "sdkconfig.h"
#include "initialize.h"

#define TAG "test"

void app_main(void)
{
    
    UNITY_BEGIN();
    TEST_ASSERT_EQUAL(ESP_OK, gpio_install_isr_service(0));
    TEST_ASSERT_EQUAL(ESP_OK, initializeLogChannel());
    unity_run_menu();
    UNITY_END();
    for (;;) {
        vTaskDelay(INT_MAX);
    }
}