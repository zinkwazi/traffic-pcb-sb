/**
 * utilities.h
 * 
 * This file contains functions that may be useful to tasks
 * contained in various other header files.
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#include "esp_assert.h"
#include "esp_debug_helpers.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "app_errors.h"

#define UTIL_TAG "utilities" // not TAG to avoid duplicate definitions warning

#define STR_HELPER(x) #x      /* STR(x) adds "" around any text expression */
#define STR(x) STR_HELPER(x)

#ifndef CONFIG_SERVER_HARDWARE_VERSION
#error "CONFIG_SERVER_HARDWARE_VERSION undefined!"
#endif

#define HARDWARE_VERSION_STR "V" STR(CONFIG_HARDWARE_VERSION) "_" STR(CONFIG_HARDWARE_REVISION)
#define SERVER_HARDWARE_VERSION_STR "V" STR(CONFIG_SERVER_HARDWARE_VERSION) "_" STR(0)
#define VERSION_STR HARDWARE_VERSION_STR "_" STR(CONFIG_FIRMWARE_VERSION)
#define VERBOSE_VERSION_STR VERSION_STR CONFIG_FIRMWARE_CONF
#define SERVER_VERSION_STR SERVER_HARDWARE_VERSION_STR "_" STR(CONFIG_SERVER_FIRMWARE_VERSION)

#define FIRMWARE_UPGRADE_URL CONFIG_FIRMWARE_UPGRADE_SERVER "/firmware/firmware" HARDWARE_VERSION_STR ".bin"

/**
 * @brief Throws a fatal error if err is not ESP_OK.
 * 
 * @param x A value of type esp_err_t.
 * @param errResources A pointer to an ErrorResources holding global error
 *                     handling resources.
 */
#define FATAL_IF_ERR(x, errResources) \
      if (x != ESP_OK) { ESP_LOGE(UTIL_TAG, "err: %d", x); esp_backtrace_print(5); throwFatalError(errResources, false); }

/**
 * @brief Throws a fatal error if x is not true.
 * 
 * @param x A bool.
 * @param errResources A pointer to an ErrorResources holding global error
 *                     handling resources.
 */
#define FATAL_IF_FALSE(x, errResources) \
      if (!x) { esp_backtrace_print(5); throwFatalError(errResources, false); } 

#endif /* UTILITIES_H_ */