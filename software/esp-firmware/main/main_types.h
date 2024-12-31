/**
 * main_types.h
 */

#ifndef MAIN_TYPES_H_
#define MAIN_TYPES_H_

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

/**
 * @brief Describes the combination of errors currently being handled.
 */
enum AppError {
    NO_ERR,
    NO_SERVER_CONNECT_ERR,
    HANDLEABLE_ERR,
    HANDLEABLE_AND_NO_SERVER_CONNECT_ERR,
    FATAL_ERR,
};

typedef enum AppError AppError;

/**
 * @brief A group of pointers to resources necessary to synchronize errors
 *        that occur in various tasks.
 * 
 * @note These errors are presented to the user through the error LED, with
 *       each task being able to present an error to the user or crash the
 *       program. Some errors are recoverable; once a task has recovered it
 *       gives control of the error LED to another task that has encountered
 *       an error until all errors have been handled or the program restarts.
 */
struct ErrorResources {
    AppError err; /*!< Indicates the errors currently being handled by the 
                       application. This should only be modified after 
                       errMutex is obtained. */
    esp_timer_handle_t errTimer; /*!< A handle to a timer that flashes the
                                      error LED if active. This should be
                                      modified only after errMutex is 
                                      obtained. */
    SemaphoreHandle_t errMutex; /*!< A handle to a mutex that guards access
                                     to err. */
};

typedef struct ErrorResources ErrorResources;

#endif /* MAIN_TYPES_H_ */