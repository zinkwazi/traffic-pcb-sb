/**
 * This app runs typical unit tests that require heavy mocking.
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
#include "Mockindicators.h"
#include "ota_config.h"
#include "utilities.h"
#include "wifi.h"
#include "mock_esp_http_client.h"
#include "wrap_esp_http_client.h"

#define TAG "test_main"

#define API_METHOD HTTP_METHOD_GET
#define API_AUTH_TYPE HTTP_AUTH_TYPE_NONE
#define RETRY_NUM 5
#define NUM_HEAP_RECORDS 300

esp_http_client_handle_t client; // externed in tests

static heap_trace_record_t traceRecord[NUM_HEAP_RECORDS]; // This buffer must be in internal RAM

void setUp(void)
{
    const esp_http_client_config_t httpConfig = {
        .host = CONFIG_DATA_SERVER,
        .path = "/",
        .auth_type = API_AUTH_TYPE,
        .method = API_METHOD,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = NULL,
        .user_data = NULL,
    };

    // macroResetOTAConfig();
    macroResetUtils();
    mock_esp_http_client_setup();

    client = ESP_HTTP_CLIENT_INIT(&httpConfig);
    TEST_ASSERT_NOT_NULL(client);
}

void tearDown(void)
{
    esp_err_t err;

    err = ESP_HTTP_CLIENT_CLEANUP(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

void app_main(void)
{
    esp_err_t err;

    UNITY_BEGIN();
    ESP_LOGE(TAG, "initializing logging");
    TEST_ASSERT_EQUAL(ESP_OK, heap_trace_init_standalone(traceRecord, NUM_HEAP_RECORDS));

    /* initialize non-volatile storage */
    err = nvs_flash_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* initialize components */
    err = initLedMatrix();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = initAppErrors();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* initialize tcp/ip stack */
    err = esp_netif_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = esp_event_loop_create_default();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    (void) esp_netif_create_default_wifi_sta();

    /* run tests */
    unity_run_all_tests();
    UNITY_END();
    unity_run_menu();
}