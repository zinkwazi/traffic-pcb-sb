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
    
    unity_run_all_tests();
    UNITY_END();
    unity_run_menu();
}