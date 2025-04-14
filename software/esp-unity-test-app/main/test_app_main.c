/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "wifi.h"
#include "pinout.h"
#include "refresh.h"

#define API_METHOD HTTP_METHOD_GET
#define API_AUTH_TYPE HTTP_AUTH_TYPE_NONE
#define RETRY_NUM 5

esp_http_client_handle_t client;

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

    client = esp_http_client_init(&httpConfig);
    TEST_ASSERT_NOT_NULL(client);
}

void tearDown(void) {
    esp_err_t err;

    err = esp_http_client_cleanup(client);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

void app_main(void)
{
    esp_err_t err;
    UNITY_BEGIN();
    
    /* initialize non-volatile storage */
    err = nvs_flash_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* initialize tcp/ip stack */
    err = esp_netif_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = esp_event_loop_create_default();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    (void) esp_netif_create_default_wifi_sta();

    /* establish wifi connection & tls */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = initWifi(CONFIG_TEST_WIFI_SSID, CONFIG_TEST_WIFI_PASSWORD);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = establishWifiConnection();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* initialize components */
    

#if CONFIG_HARDWARE_VERSION == 1
    err = matInitialize(I2C_PORT, SDA_PIN, SCL_PIN);
    TEST_ASSERT_EQUAL(ESP_OK, err);
#elif CONFIG_HARDWARE_VERSION == 2
    err = matInitializeBus1(I2C1_PORT, SDA1_PIN, SCL1_PIN);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = matInitializeBus2(I2C2_PORT, SDA2_PIN, SCL2_PIN);
    TEST_ASSERT_EQUAL(ESP_OK, err);
#else
#error "Hardware version unsupported!"
#endif
    err = initRefresh()
    
    /* run unit tests */
    unity_run_all_tests();
    UNITY_END();
    unity_run_menu();
}