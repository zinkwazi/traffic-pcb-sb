/**
 * main_types.h
 */

#ifndef MAIN_TYPES_H_
#define MAIN_TYPES_H_

#include <stddef.h>

#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#define NO_ERROR_EVENT_BIT 0x01

/**
 * @brief Describes the direction of traffic flow of a road segment.
 */
enum Direction {
    NORTH,
    SOUTH,
};

typedef enum Direction Direction;

/**
 * @brief Describes the type of LED data of stored.
 */
enum SpeedCategory {
    LIVE,
    TYPICAL,
};

typedef enum SpeedCategory SpeedCategory;

/**
 * @brief User non-volatile storage settings.
 * 
 * @note This struct is populated when user non-volatile storage settings
 *       are retrieved with retrieveNvsEntries.
 */
struct UserSettings {
    char *wifiSSID; /*!< A string containing the wifi SSID. */
    size_t wifiSSIDLen; /*!< The length of the wifiSSID string. */
    char *wifiPass; /*!< A string containing the wifi password. */
    size_t wifiPassLen; /*!< The length of the wifiPass string. */
};

typedef struct UserSettings UserSettings;

#endif /* MAIN_TYPES_H_ */