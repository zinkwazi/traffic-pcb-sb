/**
 * test_getServerSpeedsWithAddendums.c
 * 
 * White box unit tests for api_connect.c:getServerSpeedsWithAddendums.
 * 
 * Test file dependencies: None.
 * 
 * Server file dependencies: 
 *     CONFIG_API_CONN_TEST_DATA_BASE_URL/data_north_V1_0_5.csv.
 *     CONFIG_API_CONN_TEST_DATA_BASE_URL/data_north_V1_0_5.csv_add/ALL_FIRST_ADDENDUM_FILENAMES
 */

#include "api_connect_pi.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_assert.h"
#include "unity.h"
#include "sdkconfig.h"

#include "wrap_esp_http_client.h"
#include "mock_esp_http_client.h"
#include "resources/testApiConnectResources.h"

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
    DEFINE_DATA_NORTH_V1_0_5_ENDPOINT(data_north_V1_0_5);

    esp_err_t err;
    const uint32_t ledSpeedsLen = 5;
    const int retryNum = 3;
    LEDData ledSpeeds[ledSpeedsLen];

    err = getServerSpeedsWithAddendums(NULL, ledSpeedsLen, client, data_north_V1_0_5.url, retryNum);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = getServerSpeedsWithAddendums(ledSpeeds, 0, client, data_north_V1_0_5.url, retryNum);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = getServerSpeedsWithAddendums(ledSpeeds, ledSpeedsLen, NULL, data_north_V1_0_5.url, retryNum);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = getServerSpeedsWithAddendums(ledSpeeds, ledSpeedsLen, client, NULL, retryNum);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = getServerSpeedsWithAddendums(ledSpeeds, ledSpeedsLen, client, data_north_V1_0_5.url, 0);
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
    (void) test_expected_end; // unused

    DEFINE_DATA_NORTH_V1_0_5_ENDPOINT(data_north_V1_0_5);
    DEFINE_DATA_NORTH_ADD_V2_0_0_ENDPOINT(data_north_add_V2_0_0);
    DEFINE_DATA_NORTH_ADD_V1_0_5_ENDPOINT(data_north_add_V1_0_5);

    esp_err_t err;
    const uint32_t ledSpeedsLen = 326;
    const int retryNum = 3;
    LEDData ledSpeeds[ledSpeedsLen];

    /* define endpoints with expected query parameters */
    uint8_t mac[6];
    char query[MAX_QUERY_LEN];
    char northURL[MAX_URL_LEN + MAX_QUERY_LEN];
    char northAddV2URL[MAX_URL_LEN + MAX_QUERY_LEN];
    char northAddV1URL[MAX_URL_LEN + MAX_QUERY_LEN];

    err = esp_base_mac_addr_get(mac);
    snprintf(query, MAX_QUERY_LEN, "?id=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    strncpy(northURL, data_north_V1_0_5.url, MAX_URL_LEN);
    northURL[MAX_URL_LEN] = '\0';
    strncat(northURL, query, MAX_QUERY_LEN);

    strncpy(northAddV2URL, data_north_add_V2_0_0.url, MAX_URL_LEN);
    northAddV2URL[MAX_URL_LEN] = '\0';
    strncat(northAddV2URL, query, MAX_QUERY_LEN);

    strncpy(northAddV1URL, data_north_add_V1_0_5.url, MAX_URL_LEN);
    northAddV1URL[MAX_URL_LEN] = '\0';
    strncat(northAddV1URL, query, MAX_QUERY_LEN);

    const MockHttpEndpoint northV105EndpointWithParams = {
        .url = northURL,
        .responseCode = data_north_V1_0_5.responseCode,
        .response = data_north_V1_0_5.response,
        .contentLen = data_north_V1_0_5.contentLen,
    };

    const MockHttpEndpoint northAddV200EndpointWithParams = {
        .url = northAddV2URL,
        .responseCode = data_north_add_V2_0_0.responseCode,
        .response = data_north_add_V2_0_0.response,
        .contentLen = data_north_add_V2_0_0.contentLen,
    };

    const MockHttpEndpoint northAddV105EndpointWithParams = {
        .url = northAddV1URL,
        .responseCode = data_north_add_V1_0_5.responseCode,
        .response = data_north_add_V1_0_5.response,
        .contentLen = data_north_add_V1_0_5.contentLen,
    };

    err = mock_esp_http_client_add_endpoint(northV105EndpointWithParams);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(northAddV200EndpointWithParams);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = mock_esp_http_client_add_endpoint(northAddV105EndpointWithParams);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* run test */
    err = getServerSpeedsWithAddendums(ledSpeeds, ledSpeedsLen, client, data_north_V1_0_5.url, retryNum);
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