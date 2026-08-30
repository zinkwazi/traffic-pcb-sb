/**
 * test_refresh_utilities.c
 *
 * Unit tests for refresh_utilities.h.
 *
 * @note These tests run against real IS31FL3741A hardware (see the
 * test_refresh executable target), not a mock. setLEDColor()'s retry-on-
 * failure behavior can't be exercised without fault injection, so those
 * cases are deferred until a led_matrix fake (CONFIG_FAKE_LED_MATRIX)
 * exists; only the success path is covered here, using the same
 * save/set/verify/restore pattern as led_matrix's own hardware tests.
 */

#include "refresh_utilities.h"

#include <stdint.h>

#include "unity.h"

#include "esp_err.h"

#include "led_matrix.h"
#include "main_types.h"

#include "refresh_config.h"

/* An LED number guaranteed to be present on any populated V2.0/V2.1 board. */
#define TEST_LED_NUM (1)

static const Color testColor = { .red = 0x01, .green = 0x00, .blue = 0x00 };

/**
 * @brief Ensures the led_matrix component is initialized, independent of
 * test execution order. There is no deinit function, so once initialized the
 * matrix stays initialized for the rest of the test binary.
 */
static void ensureLedMatrixInitialized(void)
{
    if (getLedMatrixStatus() != ESP_OK)
    {
        TEST_ASSERT_EQUAL(ESP_OK, initLedMatrix());
    }
}

TEST_CASE("setLEDColor_setsColorAndScalingOnFirstTry", "[refresh_utilities]")
{
    ensureLedMatrixInitialized();

    uint8_t origRed, origGreen, origBlue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetColor(TEST_LED_NUM, &origRed, &origGreen, &origBlue));
    uint8_t origScaleRed, origScaleGreen, origScaleBlue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetScaling(TEST_LED_NUM, &origScaleRed, &origScaleGreen, &origScaleBlue));

    esp_err_t err = setLEDColor(TEST_LED_NUM, testColor, SET_TO_DEFAULT_BRIGHTNESS);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    uint8_t red, green, blue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetColor(TEST_LED_NUM, &red, &green, &blue));
    TEST_ASSERT_EQUAL(testColor.red, red);
    TEST_ASSERT_EQUAL(testColor.green, green);
    TEST_ASSERT_EQUAL(testColor.blue, blue);

    uint8_t scaleRed, scaleGreen, scaleBlue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetScaling(TEST_LED_NUM, &scaleRed, &scaleGreen, &scaleBlue));
    TEST_ASSERT_EQUAL(DEFAULT_SCALE, scaleRed);
    TEST_ASSERT_EQUAL(DEFAULT_SCALE, scaleGreen);
    TEST_ASSERT_EQUAL(DEFAULT_SCALE, scaleBlue);

    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, origRed, origGreen, origBlue));
    TEST_ASSERT_EQUAL(ESP_OK, matSetScaling(TEST_LED_NUM, origScaleRed, origScaleGreen, origScaleBlue));
}

TEST_CASE("setLEDColor_skipsScalingWhenNotRequested", "[refresh_utilities]")
{
    ensureLedMatrixInitialized();

    uint8_t origRed, origGreen, origBlue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetColor(TEST_LED_NUM, &origRed, &origGreen, &origBlue));
    uint8_t origScaleRed, origScaleGreen, origScaleBlue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetScaling(TEST_LED_NUM, &origScaleRed, &origScaleGreen, &origScaleBlue));

    esp_err_t err = setLEDColor(TEST_LED_NUM, testColor, DONT_SET_BRIGHTNESS);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    uint8_t red, green, blue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetColor(TEST_LED_NUM, &red, &green, &blue));
    TEST_ASSERT_EQUAL(testColor.red, red);
    TEST_ASSERT_EQUAL(testColor.green, green);
    TEST_ASSERT_EQUAL(testColor.blue, blue);

    /* scaling must be untouched, since setToDefaultBrightness was false */
    uint8_t scaleRed, scaleGreen, scaleBlue;
    TEST_ASSERT_EQUAL(ESP_OK, matGetScaling(TEST_LED_NUM, &scaleRed, &scaleGreen, &scaleBlue));
    TEST_ASSERT_EQUAL(origScaleRed, scaleRed);
    TEST_ASSERT_EQUAL(origScaleGreen, scaleGreen);
    TEST_ASSERT_EQUAL(origScaleBlue, scaleBlue);

    TEST_ASSERT_EQUAL(ESP_OK, matSetColor(TEST_LED_NUM, origRed, origGreen, origBlue));
}
