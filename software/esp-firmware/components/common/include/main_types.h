/**
 * main_types.h
 */

#ifndef MAIN_TYPES_H_
#define MAIN_TYPES_H_

#include <stddef.h>

#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "nvs.h"

#include "app_errors.h"

#define NO_ERROR_EVENT_BIT 0x01

/**
 * @brief Describes the direction of traffic flow of a road segment.
 */
typedef enum Direction {
    NORTH,
    SOUTH,
    NO_DIR,
} Direction;

/**
 * @brief Describes the type of LED data of stored.
 */
typedef enum SpeedCategory {
    LIVE,
    TYPICAL,
} SpeedCategory;

/**
 * @brief User non-volatile storage settings.
 * 
 * @note This struct is populated when user non-volatile storage settings
 *       are retrieved with retrieveNvsEntries.
 */
typedef struct UserSettings {
    char *wifiSSID; /*!< A string containing the wifi SSID. */
    size_t wifiSSIDLen; /*!< The length of the wifiSSID string. */
    char *wifiPass; /*!< A string containing the wifi password. */
    size_t wifiPassLen; /*!< The length of the wifiPass string. */
} UserSettings;

typedef struct MainTaskResources {
    nvs_handle_t nvsHandle;
    UserSettings *settings;
    esp_timer_handle_t refreshTimer;
} MainTaskResources;

typedef struct LEDData {
    uint16_t ledNum;
    /* The speed of the LED, with negative values specifying special LED types.*/
    int8_t speed;
} LEDData;

typedef struct Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Color;

#endif /* MAIN_TYPES_H_ */