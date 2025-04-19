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

/* server URLs */
#define HARDWARE_VERSION_STR "V" STR(CONFIG_HARDWARE_VERSION) "_" STR(CONFIG_HARDWARE_REVISION)
#define SERVER_HARDWARE_VERSION_STR "V" STR(CONFIG_SERVER_HARDWARE_VERSION) "_" STR(0)
#define FIRMWARE_VERSION_STR "v" STR(CONFIG_FIRMWARE_MAJOR_VERSION) "." STR(CONFIG_FIRMWARE_MINOR_VERSION) "." STR(CONFIG_FIRMWARE_PATCH_VERSION) CONFIG_FIRMWARE_CONF

#define SERVER_VERSION_STR SERVER_HARDWARE_VERSION_STR "_" STR(CONFIG_SERVER_FIRMWARE_VERSION)

#define FIRMWARE_UPGRADE_VERSION_URL CONFIG_FIRMWARE_UPGRADE_SERVER "/firmware/" HARDWARE_VERSION_STR "_version.json"
#define FIRMWARE_UPGRADE_URL CONFIG_FIRMWARE_UPGRADE_SERVER "/firmware/firmware" HARDWARE_VERSION_STR ".bin"

/* LED color configuration */
#define SLOW_RED (0xFF)
#define SLOW_GREEN (0x00)
#define SLOW_BLUE (0x00)

#define MEDIUM_RED (0x25)
#define MEDIUM_GREEN (0x09)
#define MEDIUM_BLUE (0x00)

#define FAST_RED (0x00)
#define FAST_GREEN (0x10)
#define FAST_BLUE (0x00)

#endif /* UTILITIES_H_ */