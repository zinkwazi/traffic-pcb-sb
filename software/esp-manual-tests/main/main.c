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

#define TAG "test"

#define GLOBAL_TEST_CURRENT 0x30
#define GLOBAL_POWER_TEST_CURRENT 0x80

#if CONFIG_HARDWARE_VERSION == 1

void pinout_test(void) {
    VerificationResources res;
    TEST_ASSERT_EQUAL(ESP_OK, initializeVerificationButtons(&res));
    assertHumanVerifies("Verify Toggle Button by pressing...", true, res);
    assertHumanVerifies("Verify OTA Button by pressing...", false, res);
    ESP_LOGI(TAG, "\nPress \"Toggle\" to verify, \"OTA\" to fail:\n");

    /* test status indicators */
    gpio_set_level(WIFI_LED_PIN, 1);
    gpio_set_direction(WIFI_LED_PIN, GPIO_MODE_OUTPUT);
    assertHumanVerifies("Verify Wifi LED is high...", true, res);
    gpio_set_level(WIFI_LED_PIN, 0);

    gpio_set_level(ERR_LED_PIN, 1);
    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
    assertHumanVerifies("Verify Error LED is high...", true, res);
    gpio_set_level(ERR_LED_PIN, 0);

    /* test direction indicators */
    gpio_set_level(LED_NORTH_PIN, 1);
    gpio_set_direction(LED_NORTH_PIN, GPIO_MODE_OUTPUT);
    assertHumanVerifies("Verify North LED is high...", true, res);
    gpio_set_level(LED_NORTH_PIN, 0);

    gpio_set_level(LED_EAST_PIN, 1);
    gpio_set_direction(LED_EAST_PIN, GPIO_MODE_OUTPUT);
    assertHumanVerifies("Verify East LED is high...", true, res);
    gpio_set_level(LED_EAST_PIN, 0);

    gpio_set_level(LED_SOUTH_PIN, 1);
    gpio_set_direction(LED_SOUTH_PIN, GPIO_MODE_OUTPUT);
    assertHumanVerifies("Verify South LED is high...", true, res);
    gpio_set_level(LED_SOUTH_PIN, 0);

    gpio_set_level(LED_WEST_PIN, 1);
    gpio_set_direction(LED_WEST_PIN, GPIO_MODE_OUTPUT);
    assertHumanVerifies("Verify West LED is high...", true, res);
    gpio_set_level(LED_WEST_PIN, 0);

    /* test I2C pins */
    gpio_set_level(SCL_PIN, 1);
    gpio_set_direction(SCL_PIN, GPIO_MODE_OUTPUT);
    assertHumanVerifies("Verify I2C SCL line is high...", true, res);
    gpio_set_level(SCL_PIN, 0);

    gpio_set_level(SDA_PIN, 1);
    gpio_set_direction(SDA_PIN, GPIO_MODE_OUTPUT);
    assertHumanVerifies("Verify I2C SDA line is high...", true, res);
    gpio_set_level(SDA_PIN, 0);
}

void power_test(void) {
    VerificationResources res;
    TEST_ASSERT_EQUAL(ESP_OK, initializeVerificationButtons(&res));
    TEST_ASSERT_EQUAL(ESP_OK, matInitialize(I2C_PORT, SDA_PIN, SCL_PIN));
    TEST_ASSERT_EQUAL(ESP_OK, matReset());
    TEST_ASSERT_EQUAL(ESP_OK, matSetGlobalCurrentControl(GLOBAL_TEST_CURRENT));
    TEST_ASSERT_EQUAL(ESP_OK, matSetOperatingMode(NORMAL_OPERATION));
    for (int i = 1; i < MAX_NUM_LEDS_REG; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(i, 0xFF, 0xFF, 0xFF));
        TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0xFF, 0xFF, 0xFF));
    }
    ESP_LOGI(TAG, "\nPress \"Toggle\" to verify, \"OTA\" to fail:\n");
    assertHumanVerifies("Power Draw is acceptable?", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, matReset());
}

void led_color_test(void) {
    VerificationResources res;
    TEST_ASSERT_EQUAL(ESP_OK, initializeVerificationButtons(&res));
    TEST_ASSERT_EQUAL(ESP_OK, matInitialize(I2C_PORT, SDA_PIN, SCL_PIN));
    TEST_ASSERT_EQUAL(ESP_OK, matReset());
    TEST_ASSERT_EQUAL(ESP_OK, matSetGlobalCurrentControl(GLOBAL_TEST_CURRENT));
    TEST_ASSERT_EQUAL(ESP_OK, matSetOperatingMode(NORMAL_OPERATION));
    ESP_LOGI(TAG, "\nPress \"Toggle\" to verify, \"OTA\" to fail:\n");
    for (int i = 1; i <= MAX_NUM_LEDS_REG; i++) {
        if (i == 294) {
            LEDReg reg = LEDNumToReg[i];
            TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(i, 0xFF, 0xFF, 0xFF));

            TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0xFF, 0x00, 0x00));
            ESP_LOGI(TAG, "LED %d RED  , 0x%X", i, reg.red);
            assertHumanVerifies("Verify LED...", true, res);
            
            TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0x00, 0xFF, 0x00));
            ESP_LOGI(TAG, "LED %d GREEN, 0x%X", i, reg.green);
            assertHumanVerifies("Verify LED...", true, res);

            TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0x00, 0x00, 0xFF));
            ESP_LOGI(TAG, "LED %d BLUE , 0x%X", i, reg.blue);
            assertHumanVerifies("Verify LED...", true, res);

            TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0x00, 0x00, 0x00));
        }
    }
}

#elif CONFIG_HARDWARE_VERSION == 2

void pinout_test(void) {
    VerificationResources res;
    ESP_LOGI(TAG, "\nPress \"Toggle\" to verify, \"OTA\" to fail:\n");
    assertHumanVerifies("Power Draw is acceptable?", true, res);
}

void power_test(void) {
    VerificationResources res;
    ESP_LOGI(TAG, "\nPress \"Toggle\" to verify, \"OTA\" to fail:\n");
    assertHumanVerifies("Power Draw is acceptable?", true, res);
}

void led_color_test(void) {
    VerificationResources res;
    TEST_ASSERT_EQUAL(ESP_OK, initializeVerificationButtons(&res));
    TEST_ASSERT_EQUAL(ESP_OK, matInitializeBus1(I2C1_PORT, SDA1_PIN, SCL1_PIN));
    TEST_ASSERT_EQUAL(ESP_OK, matInitializeBus2(I2C2_PORT, SDA2_PIN, SCL2_PIN));
    TEST_ASSERT_EQUAL(ESP_OK, matReset());
    TEST_ASSERT_EQUAL(ESP_OK, matSetGlobalCurrentControl(GLOBAL_TEST_CURRENT));
    TEST_ASSERT_EQUAL(ESP_OK, matSetOperatingMode(NORMAL_OPERATION));
    ESP_LOGI(TAG, "\nPress \"Toggle\" to verify, \"OTA\" to fail:\n");
    for (int i = 1; i <= MAX_NUM_LEDS_REG; i++) {
        LEDReg reg = LEDNumToReg[i];
        if (reg.matrix == MAT_NONE) {
            ESP_LOGI(TAG, "LED %d", i);
            assertHumanVerifies("Verify No LED...", true, res);
        } else {
            TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(i, 0xFF, 0xFF, 0xFF));

            TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0xFF, 0x00, 0x00));
            ESP_LOGI(TAG, "LED %d RED  , 0x%X", i, reg.red);
            assertHumanVerifies("Verify LED...", true, res);
            
            TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0x00, 0xFF, 0x00));
            ESP_LOGI(TAG, "LED %d GREEN, 0x%X", i, reg.green);
            assertHumanVerifies("Verify LED...", true, res);
    
            TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0x00, 0x00, 0xFF));
            ESP_LOGI(TAG, "LED %d BLUE , 0x%X", i, reg.blue);
            assertHumanVerifies("Verify LED...", true, res);
    
            TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0x00, 0x00, 0x00));
        }
    }
}

#else
#error "Unsupported hardware version!"
#endif

void app_main(void)
{
    UNITY_BEGIN();
    gpio_install_isr_service(0);
#if CONFIG_RUN_PINOUT_TEST == true
    RUN_TEST(pinout_test);
#endif
#if CONFIG_RUN_POWER_TEST == true
    RUN_TEST(power_test);
#endif
#if CONFIG_RUN_LED_COLOR_TEST == true
    RUN_TEST(led_color_test);
#endif
    UNITY_END();
    for (;;) {
        vTaskDelay(INT_MAX);
    }
}