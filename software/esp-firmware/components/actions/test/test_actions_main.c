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
 #include "sdkconfig.h"
 
 #include "app_errors.h"
 #include "led_matrix.h"
 #include "Mockindicators.h"
 #include "ota_config.h"
 #include "utilities.h"
 #include "wifi.h"
 
 #define TAG "test_main"
 
 #define API_METHOD HTTP_METHOD_GET
 #define API_AUTH_TYPE HTTP_AUTH_TYPE_NONE
 #define RETRY_NUM 5
 
 esp_http_client_handle_t client; // externed in tests
 
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
 
     client = esp_http_client_init(&httpConfig);
     TEST_ASSERT_NOT_NULL(client);
 
     macroResetOTAConfig();
     macroResetUtils();
 }
 
 void tearDown(void)
 {
     esp_err_t err;
 
     err = esp_http_client_cleanup(client);
     TEST_ASSERT_EQUAL(ESP_OK, err);
 }
 
 void app_main(void)
 {
     esp_err_t err;
 
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
 
     /* establish wifi connection & tls */
     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
     err = esp_wifi_init(&cfg);
     TEST_ASSERT_EQUAL(ESP_OK, err);
 
     err = initWifi(CONFIG_TEST_WIFI_SSID, CONFIG_TEST_WIFI_PASSWORD);
     TEST_ASSERT_EQUAL(ESP_OK, err);
 
 #if CONFIG_MOCK_INDICATORS
     /* ignore wifi indicator mocks */
     Mockindicators_Init();
     indicateWifiConnected_IgnoreAndReturn(ESP_OK);
     indicateWifiNotConnected_IgnoreAndReturn(ESP_OK);
 #endif
 
     err = establishWifiConnection();
     TEST_ASSERT_EQUAL(ESP_OK, err);
 
     /* run tests */
     UNITY_BEGIN();
     unity_run_all_tests();
     UNITY_END();
     unity_run_menu();
 }