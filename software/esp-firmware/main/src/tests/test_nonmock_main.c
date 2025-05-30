/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"

#include "cmock.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_debug_helpers.h"

#include "app_errors.h"
#include "led_matrix.h"
#include "pinout.h"
#include "refresh.h"
#include "wifi.h"
#include "strobe.h"
#include "strobe_task.h"
#include "wrap_esp_http_client.h"
#include "mock_esp_http_client.h"

#define API_METHOD HTTP_METHOD_GET
#define API_AUTH_TYPE HTTP_AUTH_TYPE_NONE
#define RETRY_NUM 5

esp_http_client_handle_t client;

void http_mock_fail_callback(void)
{
    esp_backtrace_print(7);
}

void setUp(void) {
    const esp_http_client_config_t httpConfig = {
        .host = CONFIG_DATA_SERVER,
        .path = "/",
        .auth_type = API_AUTH_TYPE,
        .method = API_METHOD,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = NULL,
        .user_data = NULL,
    };

    mock_esp_http_client_setup();
    mock_esp_http_client_register_fail_callback(http_mock_fail_callback);

    client = ESP_HTTP_CLIENT_INIT(&httpConfig);
    TEST_ASSERT_NOT_NULL(client);
}

void tearDown(void) {
    esp_err_t err;

    err = ESP_HTTP_CLIENT_CLEANUP(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

void app_main(void)
{
    esp_err_t err;
    UNITY_BEGIN();
    
    /* initialize non-volatile storage */
    err = nvs_flash_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    mock_esp_http_client_setup();
    mock_esp_http_client_register_fail_callback(http_mock_fail_callback);

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

    // err = initRefresh();
    // TEST_ASSERT_EQUAL(ESP_OK, err);

#if CONFIG_HARDWARE_VERSION == 1
    /* feature disabled */
#elif CONFIG_HARDWARE_VERSION == 2
    err = createStrobeTask(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, err);
#endif
    
    /* run unit tests */
    unity_run_all_tests();
    UNITY_END();
    unity_run_menu();
}