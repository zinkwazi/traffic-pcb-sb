/**
 * indicators.c
 * 
 * Contains functions for indicating non-error events.
 */

#include "indicators.h"

#include <stdint.h>

#include "sdkconfig.h"
#include "esp_err.h"

#include "app_errors.h"
#include "led_matrix.h"
#include "pinout.h"
#include "main_types.h"
#include "strobe.h"

#define V2_0_WIFI_CONNECTED_COLOR_RED (CONFIG_WHITE_RED_COMPONENT)
#define V2_0_WIFI_CONNECTED_COLOR_GREEN (CONFIG_WHITE_GREEN_COMPONENT)
#define V2_0_WIFI_CONNECTED_COLOR_BLUE (CONFIG_WHITE_BLUE_COMPONENT)

#define V2_0_OTA_UPDATE_COLOR_RED (CONFIG_WHITE_RED_COMPONENT)
#define V2_0_OTA_UPDATE_COLOR_GREEN (CONFIG_WHITE_GREEN_COMPONENT)
#define V2_0_OTA_UPDATE_COLOR_BLUE (CONFIG_WHITE_BLUE_COMPONENT)

#define V2_0_NORTHBOUND_COLOR_RED (CONFIG_WHITE_RED_COMPONENT)
#define V2_0_NORTHBOUND_COLOR_GREEN (CONFIG_WHITE_GREEN_COMPONENT)
#define V2_0_NORTHBOUND_COLOR_BLUE (CONFIG_WHITE_BLUE_COMPONENT)

#define V2_0_SOUTHBOUND_COLOR_RED (CONFIG_WHITE_RED_COMPONENT)
#define V2_0_SOUTHBOUND_COLOR_GREEN (CONFIG_WHITE_GREEN_COMPONENT)
#define V2_0_SOUTHBOUND_COLOR_BLUE (CONFIG_WHITE_BLUE_COMPONENT)

#if CONFIG_HARDWARE_VERSION == 1

/**
 * @brief Indicates to the user that wifi is connected.
 * 
 * @note For V1_0, sets wifi LED high.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * 
 * @see indicateWifiNotConnected.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateWifiConnected(void)
{
    return gpio_set_level(WIFI_LED_PIN, 1);
}

/**
 * @brief Indicates to the user that wifi is not connected.
 * 
 * @note For V1_0, sets wifi LED low.
 * 
 * @see indicateWifiConnected.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateWifiNotConnected(void)
{
    return gpio_set_level(WIFI_LED_PIN, 0);
}

/**
 * @brief Indicates to the user that an OTA update is occurring.
 * 
 * @note For V1_0, sets all direction LEDs high.
 * 
 * @see indicateOTAFailure, indicateOTASuccess.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateOTAUpdate(void)
{
    esp_err_t err;
    err = gpio_set_level(LED_NORTH_PIN, 1);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_SOUTH_PIN, 1);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_EAST_PIN, 1);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_WEST_PIN, 1);
    return err;
}

/**
 * @brief Indicates to the user that an OTA update has failed.
 * 
 * @note For V1_0, sets all direction LEDs low and throws a recoverable
 *       error that is resolved after a delay.
 * 
 * @see indicateOTAUpdate, indicateOTASuccess.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * 
 * @param[in] errRes A pointer to global error handling resources.
 * @param[in] delay The amount of time in milliseconds to keep the recoverable
 *        error in place.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateOTAFailure(ErrorResources *errRes, int32_t delay)
{
    esp_err_t err;
    err = gpio_set_level(LED_NORTH_PIN, 0);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_SOUTH_PIN, 0);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_EAST_PIN, 0);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_WEST_PIN, 0);
    if (err != ESP_OK) return err;

    throwHandleableError(errRes, false);
    vTaskDelay(pdMS_TO_TICKS(delay));
    resolveHandleableError(errRes, true, false);
    return ESP_OK;
}

/**
 * @brief Indicates to the user that an OTA update was successful.
 * 
 * @note For V1_0, no action is taken because the application should restart.
 * 
 * @see indicateOTAUpdate, indicateOTASuccess.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * 
 * @param[in] delay Unused, exists for versioning reasons.
 * 
 * @returns ESP_OK always.
 */
esp_err_t indicateOTASuccess(int32_t delay)
{
    /* no action */
    return ESP_OK;
}

/**
 * @brief Indicates to the user that northbound traffic is being displayed.
 * 
 * @note For V1_0, North and West direction LEDs are high, while South and East
 *       LEDs are low.
 * 
 * @see indicateSouthbound.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateNorthbound(void)
{
    esp_err_t err;
    err = gpio_set_level(LED_NORTH_PIN, 1);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_SOUTH_PIN, 0);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_EAST_PIN, 0);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_WEST_PIN, 1);
    return err;
}

/**
 * @brief Indicates to the user that southbound traffic is being displayed.
 * 
 * @note For V1_0, South and East direction LEDs are high, while North and West
 *       LEDs are low.
 * 
 * @see indicateNorthbound.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateSouthbound(void)
{
    esp_err_t err;
    err = gpio_set_level(LED_NORTH_PIN, 0);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_SOUTH_PIN, 1);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_EAST_PIN, 1);
    if (err != ESP_OK) return err;
    err = gpio_set_level(LED_WEST_PIN, 0);
    return err;
}

/**
 * @brief Indicates to the user the current direction of traffic.
 * 
 * @see indicateNorthbound, indicateSouthbound.
 * 
 * @requires:
 *  - initializeIndicatorLEDs executed.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateDirection(Direction dir)
{
    esp_err_t err = ESP_FAIL;
    switch(dir)
    {
        case NORTH:
            err = indicateNorthbound();
            break;
        case SOUTH:
            err = indicateSouthbound();
            break;
        default:
            err = gpio_set_level(LED_NORTH_PIN, 0);
            if (err != ESP_OK) return err;
            err = gpio_set_level(LED_SOUTH_PIN, 0);
            if (err != ESP_OK) return err;
            err = gpio_set_level(LED_EAST_PIN, 0);
            if (err != ESP_OK) return err;
            err = gpio_set_level(LED_WEST_PIN, 0);
            break;
    }
    return err;
}

#elif CONFIG_HARDWARE_VERSION == 2

/**
 * @brief Indicates to the user that wifi is connected.
 * 
 * @note For V2_0, sets wifi LED to specified wifi connected color.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * 
 * @see indicateWifiNotConnected.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateWifiConnected(void)
{
    esp_err_t err;
    err = matSetColor(WIFI_LED_NUM, 
                      V2_0_WIFI_CONNECTED_COLOR_RED,
                      V2_0_WIFI_CONNECTED_COLOR_GREEN,
                      V2_0_WIFI_CONNECTED_COLOR_BLUE);
    return err;
}

/**
 * @brief Indicates to the user that wifi is not connected.
 * 
 * @note For V1_0, sets wifi LED off.
 * 
 * @see indicateWifiConnected.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateWifiNotConnected(void)
{
    esp_err_t err;
    err = matSetColor(WIFI_LED_NUM, 0x00, 0x00, 0x00);
    return err;
}

/**
 * @brief Indicates to the user that an OTA update is available.
 * 
 * @note For V2_0, sets OTA LED to OTA success color, but strobes the LED.
 * 
 * @see indicateOTASuccess, indicateOTAUpdate.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * - strobe task created with createStrobeTask.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateOTAAvailable(void)
{
    esp_err_t err;
    err = matSetScaling(OTA_LED_NUM, 0x00, 0x00, 0x00); // let strobe task handle this
    if (err != ESP_OK) return err;
    err = matSetColor(OTA_LED_NUM,
                        CONFIG_V2_0_OTA_AVAILABLE_RED_COMPONENT,
                        CONFIG_V2_0_OTA_AVAILABLE_GREEN_COMPONENT,
                        CONFIG_V2_0_OTA_AVAILABLE_BLUE_COMPONENT);
    if (err != ESP_OK) return err;
    err = strobeRegisterLED(OTA_LED_NUM, 0x55, 0x08, 0x08, true);
    return err;
}

/**
 * @brief Indicates to the user that an OTA update is occurring.
 * 
 * @note For V2_0, sets OTA LED to specified update color.
 * 
 * @see indicateOTAFailure, indicateOTASuccess.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * - strobe task created with createStrobeTask.
 * - strobe task has a higher task priority than caller.
 * 
 * @returns ESP_OK if successful, otherwise OTA LED may be unregistered from
 *          strobe task and LED scaling may be incorrect.
 */
esp_err_t indicateOTAUpdate(void)
{
    esp_err_t err;
    err = strobeUnregisterLED(OTA_LED_NUM); // strobe task will handle this
                                            // immediately, meaning strobe task
                                            // will no longer set scaling. If
                                            // not registered, the strobe task
                                            // will ignore this request
    if (err != ESP_OK) return err; // problem sending command to queue
    err = matSetScaling(OTA_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK) return err;
    err = matSetColor(OTA_LED_NUM,
                      V2_0_OTA_UPDATE_COLOR_RED,
                      V2_0_OTA_UPDATE_COLOR_GREEN,
                      V2_0_OTA_UPDATE_COLOR_BLUE);
    return err;
}

/**
 * @brief Indicates to the user that an OTA update has failed.
 * 
 * @note For V1_0, sets all direction LEDs low and throws a recoverable
 *       error that is resolved after a delay.
 * 
 * @note For V2_0, sets OTA LED to specified failure color, then off after
 *       a delay.
 * 
 * @see indicateOTAUpdate, indicateOTASuccess.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * - strobe task created with createStrobeTask.
 * - strobe task has a higher task priority than caller.
 * 
 * @param[in] errRes Unused, exists for versioning reasons.
 * @param[in] delay The amount of time in milliseconds to keep failure
 *        indication in place.
 * 
 * @returns ESP_OK if successful, otherwise OTA LED may be unregistered from
 *          strobe task and LED scaling may be incorrect.
 */
esp_err_t indicateOTAFailure(ErrorResources *errRes, int32_t delay)
{
    esp_err_t err;
    err = strobeUnregisterLED(OTA_LED_NUM); // strobe task will handle this
                                            // immediately, meaning strobe task
                                            // will no longer set scaling. If
                                            // not registered, the strobe task
                                            // will ignore this request
    if (err != ESP_OK) return err; // problem sending command to queue
    err = matSetScaling(OTA_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK) return err;
    err = matSetColor(OTA_LED_NUM,
                      CONFIG_V2_0_OTA_FAILURE_RED_COMPONENT,
                      CONFIG_V2_0_OTA_FAILURE_GREEN_COMPONENT,
                      CONFIG_V2_0_OTA_FAILURE_BLUE_COMPONENT);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(delay));
    err = matSetColor(OTA_LED_NUM, 0x00, 0x00, 0x00);
    return err;
}

/**
 * @brief Indicates to the user that an OTA update was successful.
 * 
 * @note For V2_0, OTA LED is set to specified color, then off after a delay.
 * 
 * @see indicateOTAUpdate, indicateOTASuccess.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * - strobe task created with createStrobeTask.
 * - strobe task has a higher task priority than caller.
 * 
 * @param[in] delay The amount of time in milliseconds to keep success
 *        indication in place.
 * 
 * @returns ESP_OK if successful, otherwise OTA LED may be unregistered from
 *          strobe task and LED scaling may be incorrect.
 */
esp_err_t indicateOTASuccess(int32_t delay)
{
    esp_err_t err;
    err = strobeUnregisterLED(OTA_LED_NUM); // strobe task will handle this
                                            // immediately, meaning strobe task
                                            // will no longer set scaling. If
                                            // not registered, the strobe task
                                            // will ignore this request
    if (err != ESP_OK) return err; // problem sending command to queue
    err = matSetScaling(OTA_LED_NUM, 0xFF, 0xFF, 0xFF);
    if (err != ESP_OK) return err;
    err = matSetColor(OTA_LED_NUM,
                      CONFIG_V2_0_OTA_SUCCESS_RED_COMPONENT,
                      CONFIG_V2_0_OTA_SUCCESS_GREEN_COMPONENT,
                      CONFIG_V2_0_OTA_SUCCESS_BLUE_COMPONENT);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(delay));
    err = matSetColor(OTA_LED_NUM, 0x00, 0x00, 0x00);
    return err;
}

/**
 * @brief Indicates to the user that northbound traffic is being displayed.
 * 
 * @note For V2_0, North and West direction LEDs are set to specified color, 
 *       while South and East LEDs are off.
 * 
 * @see indicateSouthbound.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateNorthbound(void)
{
    esp_err_t err;
    err = matSetColor(NORTH_LED_NUM,
                      V2_0_NORTHBOUND_COLOR_RED,
                      V2_0_NORTHBOUND_COLOR_GREEN,
                      V2_0_NORTHBOUND_COLOR_BLUE);
    if (err != ESP_OK) return err;
    err = matSetColor(WEST_LED_NUM,
                      V2_0_NORTHBOUND_COLOR_RED,
                      V2_0_NORTHBOUND_COLOR_GREEN,
                      V2_0_NORTHBOUND_COLOR_BLUE);
    if (err != ESP_OK) return err;
    err = matSetColor(SOUTH_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK) return err;
    err = matSetColor(EAST_LED_NUM, 0x00, 0x00, 0x00);
    return err;
}

/**
 * @brief Indicates to the user that southbound traffic is being displayed.
 * 
 * @note For V1_0, South and East direction LEDs are set to specified color, 
 *       while North and West LEDs are low.
 * 
 * @see indicateNorthbound.
 * 
 * @requires:
 * - initializeIndicatorLEDs executed.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateSouthbound(void)
{
    esp_err_t err;
    err = matSetColor(SOUTH_LED_NUM,
                      V2_0_SOUTHBOUND_COLOR_RED,
                      V2_0_SOUTHBOUND_COLOR_GREEN,
                      V2_0_SOUTHBOUND_COLOR_BLUE);
    if (err != ESP_OK) return err;
    err = matSetColor(EAST_LED_NUM,
                      V2_0_SOUTHBOUND_COLOR_RED,
                      V2_0_SOUTHBOUND_COLOR_GREEN,
                      V2_0_SOUTHBOUND_COLOR_BLUE);
    if (err != ESP_OK) return err;
    err = matSetColor(NORTH_LED_NUM, 0x00, 0x00, 0x00);
    if (err != ESP_OK) return err;
    err = matSetColor(WEST_LED_NUM, 0x00, 0x00, 0x00);
    return err;
}

/**
 * @brief Indicates to the user the current direction of traffic.
 * 
 * @see indicateNorthbound, indicateSouthbound.
 * 
 * @requires:
 *  - initializeIndicatorLEDs executed.
 * 
 * @returns ESP_OK if successful.
 */
esp_err_t indicateDirection(Direction dir)
{
    esp_err_t err = ESP_FAIL;
    switch(dir)
    {
        case NORTH:
            err = indicateNorthbound();
            break;
        case SOUTH:
            err = indicateSouthbound();
            break;
        default:
            err = matSetColor(NORTH_LED_NUM, 0x00, 0x00, 0x00);
            if (err != ESP_OK) return err;
            err = matSetColor(SOUTH_LED_NUM, 0x00, 0x00, 0x00);
            if (err != ESP_OK) return err;
            err = matSetColor(EAST_LED_NUM, 0x00, 0x00, 0x00);
            if (err != ESP_OK) return err;
            err = matSetColor(WEST_LED_NUM, 0x00, 0x00, 0x00);
            break;
    }
    return err;
}

#else
#error "Unsupported hardware version!"
#endif

 