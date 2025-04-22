/**
 * test_power.c
 * 
 * A manual test that verifies that the maximum power draw is acceptable.
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

TEST_CASE("power", "[manual]")
{
    esp_err_t err;
    VerificationResources res;
    TEST_ASSERT_EQUAL(ESP_OK, initializeVerificationButtons(&res));
    (void) initLedMatrix(); // this will be called potentially more than once if the test is rerun
    TEST_ASSERT_EQUAL(ESP_OK, matReset());
    TEST_ASSERT_EQUAL(ESP_OK, matSetGlobalCurrentControl(GLOBAL_TEST_CURRENT));
    TEST_ASSERT_EQUAL(ESP_OK, matSetOperatingMode(NORMAL_OPERATION));
    for (int i = 1; i <= MAX_NUM_LEDS_REG; i++) {
#if CONFIG_HARDWARE_VERSION == 2
        if (i == 46) continue; // led 46 is not present on device
#endif
        TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(i, 0xFF, 0xFF, 0xFF));
        TEST_ASSERT_EQUAL(ESP_OK, matSetColor(i, 0xFF, 0xFF, 0xFF));
    }
    ESP_LOGI(TAG, "\nPress \"Toggle\" to verify, \"OTA\" to fail:\n");
    err = humanVerifies("Power Draw is acceptable?", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matReset());
}
