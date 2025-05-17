/**
 * test_action_updateData.c
 * 
 * Unit tests for actions.c:handleActionUpdateData.
 * 
 * Test file dependencies: None.
 * 
 * Server file dependencies:
 *      None
 */

#include "unity.h"

#include "esp_heap_trace.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#include "actions.h"
#include "actions_pi.h"
#include "utilities.h"
#include "esp_http_client.h"

#if CONFIG_ACTIONS_MAIN == 1
#include "Mockota.h"
#include "Mockindicators.h"
#include "Mockhttp_wrap.h"
#include "Mockapp_nvs.h"
#include "Mockrefresh.h"
#include "Mocktraffic_data.h"
#elif CONFIG_ACTIONS_MAIN == 2
#include "Mockhttp_wrap.h"
#endif

#define URL_BASE CONFIG_ACTIONS_TEST_DATA_SERVER CONFIG_ACTIONS_TEST_DATA_BASE_URL

#define CLIENT_MAGIC_NUM 0x63F2

#if CONFIG_HARDWARE_VERSION != 1 // specification does not include this for V1_X due to memory constraints

/**
 * This app runs typical unit tests that require heavy mocking.
 * 
 * CONFIG_MOCK_HTTP_WRAP=y
 * CONFIG_MOCK_OTA=y
 * CONFIG_MOCK_INDICATORS=y
 * CONFIG_MOCK_APP_ERRORS=y
 * CONFIG_MOCK_APP_NVS=y
 * CONFIG_MOCK_TRAFFIC_DATA=y
 * CONFIG_MOCK_REFRESH=y # to avoid util extern macro conflicts
 */
#if CONFIG_ACTIONS_MAIN == 1

static bool northUpdated, southUpdated;
static LEDData *northArr, *southArr;
static esp_http_client_handle_t sClient;

void resetDataMockState(void)
{
    northUpdated = false;
    southUpdated = false;
    northArr = NULL;
    southArr = NULL;
    sClient = NULL;
}

esp_http_client_handle_t initHttpClientMock(int num_calls)
{

    TEST_ASSERT_EQUAL(NULL, sClient);
    TEST_ASSERT_EQUAL(0, num_calls);

    sClient = (esp_http_client_handle_t) CLIENT_MAGIC_NUM;
    return sClient;
}

esp_err_t httpClientCleanupMock(esp_http_client_handle_t client, int num_calls)
{
    TEST_ASSERT_NOT_EQUAL(NULL, sClient);
    TEST_ASSERT_EQUAL(sClient, client);
    TEST_ASSERT_EQUAL(0, num_calls);

    sClient = NULL;
    return ESP_OK;
}

esp_err_t refreshDataMock(LEDData* data, esp_http_client_handle_t client, Direction dir, SpeedCategory category, int num_calls)
{
    TEST_ASSERT_NOT_EQUAL(NULL, data);
    TEST_ASSERT_NOT_EQUAL(NULL, sClient);
    TEST_ASSERT_EQUAL(sClient, client); // asserts initHttpClient called
    TEST_ASSERT_EQUAL(LIVE, category);
    TEST_ASSERT_LESS_OR_EQUAL(1, num_calls);

    switch (dir)
    {
        case NORTH:
            TEST_ASSERT_EQUAL(NULL, northArr); // refreshDataMock called already
            northArr = data;
            break;
        case SOUTH:
            TEST_ASSERT_EQUAL(NULL, southArr); // refreshDataMock called already
            southArr = data;
            break;
        default:
            TEST_FAIL();
    }

    return ESP_OK;
}

esp_err_t updateTrafficDataMock(LEDData* data, size_t dataSize, Direction dir, SpeedCategory category, int num_calls)
{
    TEST_ASSERT_NOT_EQUAL(NULL, data);
    TEST_ASSERT_EQUAL(MAX_NUM_LEDS_REG, dataSize);
    TEST_ASSERT_EQUAL(LIVE, category);
    TEST_ASSERT_LESS_OR_EQUAL(1, num_calls);

    switch(dir)
    {
        case NORTH:
            TEST_ASSERT_NOT_EQUAL(NULL, northArr);
            TEST_ASSERT_EQUAL(northArr, data);
            northArr = NULL; // ensure this is not called many times in a row
            northUpdated = true;
            break;
        case SOUTH:
            TEST_ASSERT_NOT_EQUAL(NULL, southArr);
            TEST_ASSERT_EQUAL(southArr, data);
            southArr = NULL; // ensure this is not called many times in a row
            southUpdated = true;
            break;
        default:
            TEST_FAIL();
    }

    return ESP_OK;
}

/**
 * Tests that handleActionUpdateData does not switch north and south data.
 * 
 * Test case dependencies: None.
 */
TEST_CASE("updateData_notSwitched", "[actions]")
{
    esp_err_t err;

    /* setup mocks */
    Mockhttp_wrap_Init(); // initializes all mocks
    resetDataMockState();

    initHttpClient_Stub(initHttpClientMock);
    wrap_http_client_cleanup_Stub(httpClientCleanupMock);
    refreshData_Stub(refreshDataMock);
    updateTrafficData_Stub(updateTrafficDataMock);
    borrowTrafficData_ExpectAndReturn(LIVE, portMAX_DELAY, ESP_OK);
    borrowTrafficData_IgnoreArg_xTicksToWait();
    releaseTrafficData_ExpectAndReturn(LIVE, ESP_OK);

    /* run test */
    err = handleActionUpdateData();

    /* verify results */
    TEST_ASSERT_EQUAL(ESP_OK, err);
    Mockhttp_wrap_Verify();
    Mocktraffic_data_Verify();
    Mockrefresh_Verify();
    TEST_ASSERT_EQUAL(true, northUpdated);
    TEST_ASSERT_EQUAL(true, southUpdated);
}

/**
 * This app runs tests that check for memory leaks, which requires
 * few mocks.
 * 
 * CONFIG_MOCK_HTTP_WRAP=y
 */
#elif CONFIG_ACTIONS_MAIN == 2

TEST_CASE("updateData_memoryLeak", "[actions]")
{
    /* setup mocks */
}

#endif /* CONFIG_ACTIONS_MAIN */
#endif /* CONFIG_HARDWARE_VERSION != 1 */