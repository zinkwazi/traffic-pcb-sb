/**
 * test_getServerSpeedsWithAddendums.c
 * 
 * White box unit tests for api_connect.c:getServerSpeedsWithAddendums.
 * 
 * Test file dependencies: None.
 * 
 * Server file dependencies: 
 *     CONFIG_TEST_DATA_BASE_URL/data_north_V1_0_5.csv.
 *     CONFIG_TEST_DATA_BASE_URL/data_north_V1_0_5.csv_add/ALL_FIRST_ADDENDUM_FILENAMES
 */

#include "api_connect_pi.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_err.h"
#include "esp_assert.h"
#include "unity.h"
#include "sdkconfig.h"

extern esp_http_client_handle_t client;

#define TAG "test"

/* The test function is only included when this is true */
// #if USE_ADDENDUMS == true

/**
 * Tests input guards.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("getServerSpeedsWithAddendums_inputGuards", "[api_connect]")
{
    const char *URL = CONFIG_DATA_SERVER CONFIG_TEST_DATA_BASE_URL "/data_north_V1_0_5.csv";
    esp_err_t err;
    const uint32_t ledSpeedsLen = 5;
    const int retryNum = 3;
    LEDData ledSpeeds[ledSpeedsLen];

    err = getServerSpeedsWithAddendums(NULL, ledSpeedsLen, client, URL, retryNum);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = getServerSpeedsWithAddendums(ledSpeeds, 0, client, URL, retryNum);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = getServerSpeedsWithAddendums(ledSpeeds, ledSpeedsLen, NULL, URL, retryNum);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = getServerSpeedsWithAddendums(ledSpeeds, ledSpeedsLen, client, NULL, retryNum);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = getServerSpeedsWithAddendums(ledSpeeds, ledSpeedsLen, client, URL, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

/**
 * Tests typical use case.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("getServerSpeedsWithAddendums_typical", "[api_connect]")
{
    extern const uint8_t test_expected_start[] asm("_binary_data_north_V1_0_3_dat_start");
    extern const uint8_t test_expected_end[] asm("_binary_data_north_V1_0_3_dat_end");

    const char *URL = CONFIG_DATA_SERVER CONFIG_TEST_DATA_BASE_URL "/data_north_V1_0_5.csv";
    esp_err_t err;
    const uint32_t ledSpeedsLen = 326;
    const int retryNum = 3;
    LEDData ledSpeeds[ledSpeedsLen];

    err = getServerSpeedsWithAddendums(ledSpeeds, ledSpeedsLen, client, URL, retryNum);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    for (uint32_t ndx = 0; ndx < ledSpeedsLen; ndx++)
    {
        uint16_t ledNum = ledSpeeds[ndx].ledNum;
        if (ledNum == 1)
        {
            TEST_ASSERT_EQUAL(100, ledSpeeds[ndx].speed);
        } else if (ledNum == 2)
        {
            TEST_ASSERT_EQUAL(99, ledSpeeds[ndx].speed);
        } else if (ledNum == 3)
        {
            TEST_ASSERT_EQUAL(98, ledSpeeds[ndx].speed);
        } else
        {
            if (ledSpeeds[ndx].speed != test_expected_start[ledNum])
            {
                ESP_LOGI(TAG, "led: %u", ledSpeeds[ndx].ledNum);
            }
            TEST_ASSERT_EQUAL(test_expected_start[ledNum], ledSpeeds[ndx].speed);
        }
    }
}

// #endif