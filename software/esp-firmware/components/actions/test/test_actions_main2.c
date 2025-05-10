/**
 * This app runs tests that check for memory leaks, which requires
 * few mocks.
 */

#include "unity.h"

#include "esp_heap_trace.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "app_errors.h"
#include "led_matrix.h"
#include "ota_config.h"
#include "utilities.h"
#include "wifi.h"

#define TAG "test_main"

#define API_METHOD HTTP_METHOD_GET
#define API_AUTH_TYPE HTTP_AUTH_TYPE_NONE
#define RETRY_NUM 5

#define NUM_HEAP_RECORDS 100

static heap_trace_record_t traceRecord[NUM_HEAP_RECORDS]; // This buffer must be in internal RAM

void setUp(void)
{
}

void tearDown(void)
{
}

void app_main(void)
{
    esp_err_t err;

    UNITY_BEGIN();
    TEST_ASSERT_EQUAL(ESP_OK, heap_trace_init_standalone(traceRecord, NUM_HEAP_RECORDS));

    /* initialize non-volatile storage */
    err = nvs_flash_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* initialize components */
    err = esp_event_loop_create_default();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = initLedMatrix();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = initAppErrors();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* run tests */
    unity_run_all_tests();
    UNITY_END();
    unity_run_menu();
}