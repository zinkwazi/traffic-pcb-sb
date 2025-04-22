/**
 * test_ledcolor.c
 * 
 * A manual test that verifies all LEDs produce the correct color.
 */

#include "unity.h"

#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "led_matrix.h"
#include "led_registers.h"
#include "pinout.h"
#include "verifier.h"

#define TAG "test"

#define GLOBAL_TEST_CURRENT 0x30
#define GLOBAL_POWER_TEST_CURRENT 0x80

TEST_CASE("ledcolor_tot", "[manual]")
{
    esp_err_t err;
    VerificationResources res;
    fflush(stdout);
    TEST_ASSERT_EQUAL(ESP_OK, initializeVerificationButtons(&res));
    (void) initLedMatrix(); // this will be called potentially more than once if the test is rerun
    TEST_ASSERT_EQUAL(ESP_OK, matReset());
    TEST_ASSERT_EQUAL(ESP_OK, matSetGlobalCurrentControl(GLOBAL_TEST_CURRENT));
    TEST_ASSERT_EQUAL(ESP_OK, matSetOperatingMode(NORMAL_OPERATION));
    ESP_LOGI(TAG, "\nPress \"Toggle\" to verify, \"OTA\" to fail:\n");
    for (int i = 1; i <= MAX_NUM_LEDS_REG; i++)
    {
#if CONFIG_HARDWARE_VERSION == 2
        if (i == 46) continue; // not present on board
#endif
        TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(i, 0x22, 0x22, 0x22));
    }

    /* verify red leds */
    for (int i = 1; i <= MAX_NUM_LEDS_REG; i++)
    {
#if CONFIG_HARDWARE_VERSION == 2
        if (i == 46) continue; // not present on board
#endif
        TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0xFF, 0x00, 0x00));
    }

    err = humanVerifies("Verify all LEDs red...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }

    /* verify green leds */
    for (int i = 1; i <= MAX_NUM_LEDS_REG; i++)
    {
#if CONFIG_HARDWARE_VERSION == 2
        if (i == 46) continue; // not present on board
#endif
        TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0x00, 0xFF, 0x00));
    }

    err = humanVerifies("Verify all LEDs green...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }

    /* verify blue leds */
    for (int i = 1; i <= MAX_NUM_LEDS_REG; i++)
    {
#if CONFIG_HARDWARE_VERSION == 2
        if (i == 46) continue; // not present on board
#endif
        TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0x00, 0x00, 0xFF));
    }

    err = humanVerifies("Verify all LEDs blue...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matReset());
}

TEST_CASE("ledcolor_reg", "[manual]")
{
    esp_err_t err;
    VerificationResources res;
    fflush(stdout);
    TEST_ASSERT_EQUAL(ESP_OK, initializeVerificationButtons(&res));
    (void) initLedMatrix(); // this will be called potentially more than once if the test is rerun
    TEST_ASSERT_EQUAL(ESP_OK, matReset());
    TEST_ASSERT_EQUAL(ESP_OK, matSetGlobalCurrentControl(GLOBAL_TEST_CURRENT));
    TEST_ASSERT_EQUAL(ESP_OK, matSetOperatingMode(NORMAL_OPERATION));
    ESP_LOGI(TAG, "\nPress \"Toggle\" to verify, \"OTA\" to fail:\n");
    for (int i = 1; i <= MAX_NUM_LEDS_REG; i++) {
#if CONFIG_HARDWARE_VERSION == 2
        if (i == 46) continue; // not present on board
#endif
        LEDReg reg = LEDNumToReg[i - 1];
        TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(i, 0xFF, 0xFF, 0xFF));

        TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0xFF, 0x00, 0x00));
        ESP_LOGI(TAG, "LED %d RED  , 0x%X", i, reg.red);
        err = humanVerifies("Verify LED...", true, res);
        if (err != ESP_OK)
        {
            (void) matReset();
            TEST_ASSERT_EQUAL(ESP_OK, err);
        }
        
        TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0x00, 0xFF, 0x00));
        ESP_LOGI(TAG, "LED %d GREEN, 0x%X", i, reg.green);
        err = humanVerifies("Verify LED...", true, res);
        if (err != ESP_OK)
        {
            (void) matReset();
            TEST_ASSERT_EQUAL(ESP_OK, err);
        }

        TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0x00, 0x00, 0xFF));
        ESP_LOGI(TAG, "LED %d BLUE , 0x%X", i, reg.blue);
        err = humanVerifies("Verify LED...", true, res);
        if (err != ESP_OK)
        {
            (void) matReset();
            TEST_ASSERT_EQUAL(ESP_OK, err);
        }

        TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0x00, 0x00, 0x00));
    }
    TEST_ASSERT_EQUAL(ESP_OK, matReset());
}