/**
 * test_pinout.c
 * 
 * A manual test that verifies the pinout of the project is correct.
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

#if CONFIG_HARDWARE_VERSION == 1

TEST_CASE("pinout", "[manual]")
{
    esp_err_t err;
    VerificationResources res;
    TEST_ASSERT_EQUAL(ESP_OK, initializeVerificationButtons(&res));
    err = humanVerifies("Verify Toggle Button by pressing it...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = humanVerifies("Verify OTA Button by pressing it...", false, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    ESP_LOGI(TAG, "\nPress \"Toggle\" to verify, \"OTA\" to fail:\n");

    /* test voltage indicators */
    err = humanVerifies("Verify 5v LED is on...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = humanVerifies("Verify 3.3v LED is on...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* test I2C pins */
    gpio_set_level(SCL_PIN, 1);
    gpio_set_direction(SCL_PIN, GPIO_MODE_OUTPUT);
    err = humanVerifies("Verify I2C SCL line is high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(SCL_PIN, 0);

    gpio_set_level(SDA_PIN, 1);
    gpio_set_direction(SDA_PIN, GPIO_MODE_OUTPUT);
    err = humanVerifies("Verify I2C SDA line is high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(SDA_PIN, 0);

    /* test status indicators */
    gpio_set_level(WIFI_LED_PIN, 1);
    gpio_set_direction(WIFI_LED_PIN, GPIO_MODE_OUTPUT);
    err = humanVerifies("Verify Wifi LED is high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(WIFI_LED_PIN, 0);

    gpio_set_level(ERR_LED_PIN, 1);
    gpio_set_direction(ERR_LED_PIN, GPIO_MODE_OUTPUT);
    err = humanVerifies("Verify Error LED is high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(ERR_LED_PIN, 0);

    /* test direction indicators */
    gpio_set_level(LED_NORTH_PIN, 1);
    gpio_set_direction(LED_NORTH_PIN, GPIO_MODE_OUTPUT);
    err = humanVerifies("Verify North LED is high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(LED_NORTH_PIN, 0);

    gpio_set_level(LED_EAST_PIN, 1);
    gpio_set_direction(LED_EAST_PIN, GPIO_MODE_OUTPUT);
    err = humanVerifies("Verify East LED is high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(LED_EAST_PIN, 0);

    gpio_set_level(LED_SOUTH_PIN, 1);
    gpio_set_direction(LED_SOUTH_PIN, GPIO_MODE_OUTPUT);
    err = humanVerifies("Verify South LED is high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(LED_SOUTH_PIN, 0);

    gpio_set_level(LED_WEST_PIN, 1);
    gpio_set_direction(LED_WEST_PIN, GPIO_MODE_OUTPUT);
    err = humanVerifies("Verify West LED is high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(LED_WEST_PIN, 0);
}

#elif CONFIG_HARDWARE_VERSION == 2

TEST_CASE("pinout", "[manual]")
{
    esp_err_t err;
    VerificationResources res;
    TEST_ASSERT_EQUAL(ESP_OK, initializeVerificationButtons(&res));
    err = humanVerifies("Verify Toggle Button by pressing it...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = humanVerifies("Verify Update Button by pressing it...", false, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    ESP_LOGI(TAG, "\nPress \"Toggle\" to verify, \"Update\" to fail:\n");

    /* test voltage indicators */
    err = humanVerifies("Verify 5v indicator is on...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = humanVerifies("Verify 3.3v indicator is on...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* test I2C pins */
    gpio_set_direction(SDA1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SCL2_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SDA2_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SCL2_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SDA1_PIN, 0);
    gpio_set_level(SCL2_PIN, 0);
    gpio_set_level(SDA2_PIN, 0);
    gpio_set_level(SCL2_PIN, 0);

    gpio_set_level(SDA1_PIN, 1);
    err = humanVerifies("Verify SDA1 line high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(SDA1_PIN, 0);

    gpio_set_level(SCL1_PIN, 1);
    err = humanVerifies("Verify SCL1 line high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(SCL1_PIN, 0);

    gpio_set_level(SDA2_PIN, 1);
    err = humanVerifies("Verify SDA2 line high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(SDA2_PIN, 0);

    gpio_set_level(SCL2_PIN, 1);
    err = humanVerifies("Verify SCL2 line high...", true, res);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    gpio_set_level(SCL2_PIN, 0);

    /* setup matrices */
    (void) initLedMatrix(); // this will be called potentially more than once if the test is rerun
    TEST_ASSERT_EQUAL(ESP_OK, matReset());
    TEST_ASSERT_EQUAL(ESP_OK, matSetGlobalCurrentControl(GLOBAL_TEST_CURRENT));
    TEST_ASSERT_EQUAL(ESP_OK, matSetOperatingMode(NORMAL_OPERATION));

    /* test status indicators */
    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(WIFI_LED_NUM, 0x22, 0x22, 0x22));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(WIFI_LED_NUM, 0xFF, 0xFF, 0xFF));
    err = humanVerifies("Verify Wifi LED is white...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(WIFI_LED_NUM, 0x00, 0x00, 0x00));

    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(OTA_LED_NUM, 0x22, 0x22, 0x22));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(OTA_LED_NUM, 0xFF, 0xFF, 0xFF));
    err = humanVerifies("Verify OTA LED is white...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(OTA_LED_NUM, 0x00, 0x00, 0x00));

    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(ERROR_LED_NUM, 0x22, 0x22, 0x22));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(ERROR_LED_NUM, 0xFF, 0xFF, 0xFF));
    err = humanVerifies("Verify Error LED is white...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(ERROR_LED_NUM, 0x00, 0x00, 0x00));

    /* test color legend */
    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(HEAVY_LED_NUM, 0x22, 0x22, 0x22));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(HEAVY_LED_NUM, 0xFF, 0xFF, 0xFF));
    err = humanVerifies("Verify Heavy LED is white...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(HEAVY_LED_NUM, 0x00, 0x00, 0x00));

    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(MEDIUM_LED_NUM, 0x22, 0x22, 0x22));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(MEDIUM_LED_NUM, 0xFF, 0xFF, 0xFF));
    err = humanVerifies("Verify Medium LED is white...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(MEDIUM_LED_NUM, 0x00, 0x00, 0x00));

    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(LIGHT_LED_NUM, 0x22, 0x22, 0x22));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(LIGHT_LED_NUM, 0xFF, 0xFF, 0xFF));
    err = humanVerifies("Verify Light LED is white...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(LIGHT_LED_NUM, 0x00, 0x00, 0x00));

    /* test direction indicators */
    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(NORTH_LED_NUM, 0x22, 0x22, 0x22));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(NORTH_LED_NUM, 0xFF, 0xFF, 0xFF));
    err = humanVerifies("Verify North LED is white...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(NORTH_LED_NUM, 0x00, 0x00, 0x00));
    
    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(EAST_LED_NUM, 0x22, 0x22, 0x22));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(EAST_LED_NUM, 0xFF, 0xFF, 0xFF));
    err = humanVerifies("Verify East LED is white...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(EAST_LED_NUM, 0x00, 0x00, 0x00));

    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(SOUTH_LED_NUM, 0x22, 0x22, 0x22));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(SOUTH_LED_NUM, 0xFF, 0xFF, 0xFF));
    err = humanVerifies("Verify South LED is white...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(SOUTH_LED_NUM, 0x00, 0x00, 0x00));

    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(WEST_LED_NUM, 0x22, 0x22, 0x22));
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(WEST_LED_NUM, 0xFF, 0xFF, 0xFF));
    err = humanVerifies("Verify West LED is white...", true, res);
    if (err != ESP_OK)
    {
        (void) matReset();
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }
    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(WEST_LED_NUM, 0x00, 0x00, 0x00));

    TEST_ASSERT_EQUAL(ESP_OK, matReset());
}
#else
#error "Unsupported hardware version!"
#endif