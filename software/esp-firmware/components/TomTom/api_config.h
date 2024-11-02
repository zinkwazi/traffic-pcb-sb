/**
 * api_config.h
 * 
 * Tells the tomtom component a few things about the api and
 * wifi network it is using.Note that an API key and wifi
 * ssid/password must be written to a file named 'secrets.h'
 * and NOT included in Git.
 * 
 * Necessary 'secrets.h' definitions:
 * - SECRET_WIFI_SSID (string): the name of the wifi network to connect to
 * - SECRET_WIFI_PASS (string): the password of the wifi network to connect to
 * - SECRET_API_KEY (string): the API key to provide to tomtom queries
 */

#ifndef APICONFIG_H_
#define APICONFIG_H_

/* IDF component includes */
#include "esp_wifi.h"
#include "lwip/sys.h"
#include "esp_http_client.h"

/* Tomtom component includes */
#include "secrets.h"
    
#define USE_FAKE_DATA true

/* Secrets from 'secrets.h' */
#define WIFI_SSID SECRET_WIFI_SSID
#define WIFI_PASS SECRET_WIFI_PASS
#define API_KEY SECRET_API_KEY

/* Wifi configuration */
#define WIFI_SCAN_METHOD WIFI_FAST_SCAN
#define WIFI_AUTH_MODE WIFI_AUTH_WPA2_PSK

/* API configuration */
#define API_ENDPOINT_URL "/traffic/services/4/flowSegmentData/"
#define API_METHOD HTTP_METHOD_GET
#define API_AUTH_TYPE HTTP_AUTH_TYPE_NONE
#define API_UNIT "mph" /* accepts "mph" and "kmph" */
#define API_SEND_OPENLR "true" /* accepts "true" and "false" */
#define API_STYLE "relative0" /* shouldn't affect flowSegmentData */
#define DEFAULT_POINT "34.420842,-119.702440" /* Outside Open */

/* Connection method configuration */
#define MAX_RETRY_WIFI_CONNECT (10)
#define WIFI_CONNECTED_BIT (BIT0) /* wifi event group bit */
#define WIFI_FAIL_BIT (BIT1) /* wifi event group bit */

#endif /* APICONFIG_H_ */