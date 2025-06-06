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

/**
 * These are default definitions for macros below. Default macros exist as
 * an indirect layer in order to allow extern variables to be set to the
 * default macro value during initialization, but be changed during testing.
 */

#define DEF_HARDWARE_VERSION_STR "V" STR(CONFIG_HARDWARE_VERSION) "_" STR(CONFIG_HARDWARE_REVISION)
#define DEF_SERVER_HARDWARE_VERSION_STR "V" STR(CONFIG_SERVER_HARDWARE_VERSION) "_" STR(0)
#define DEF_FIRMWARE_VERSION_STR "v" STR(CONFIG_FIRMWARE_MAJOR_VERSION) "." STR(CONFIG_FIRMWARE_MINOR_VERSION) "." STR(CONFIG_FIRMWARE_PATCH_VERSION) CONFIG_FIRMWARE_CONF
#define DEF_SERVER_VERSION_STR DEF_SERVER_HARDWARE_VERSION_STR "_" STR(CONFIG_SERVER_FIRMWARE_VERSION)
#define DEF_FIRMWARE_UPGRADE_VERSION_URL CONFIG_FIRMWARE_UPGRADE_SERVER "/firmware/" DEF_HARDWARE_VERSION_STR "_version.json"
#define DEF_FIRMWARE_UPGRADE_URL CONFIG_FIRMWARE_UPGRADE_SERVER "/firmware/firmware" DEF_HARDWARE_VERSION_STR ".bin"


#ifndef CONFIG_UTILS_EXTERN_MACROS

#define HARDWARE_VERSION_STR DEF_HARDWARE_VERSION_STR
#define SERVER_HARDWARE_VERSION_STR DEF_SERVER_HARDWARE_VERSION_STR
#define FIRMWARE_VERSION_STR DEF_FIRMWARE_VERSION_STR

#define SERVER_VERSION_STR DEF_SERVER_VERSION_STR

#define FIRMWARE_UPGRADE_VERSION_URL DEF_FIRMWARE_UPGRADE_VERSION_URL
#define FIRMWARE_UPGRADE_URL DEF_FIRMWARE_UPGRADE_URL

#else /* CONFIG_UTILS_EXTERN_MACROS */

extern const char *HARDWARE_VERSION_STR;
extern const char *SERVER_HARDWARE_VERSION_STR;
extern const char *FIRMWARE_VERSION_STR;
extern const char *SERVER_VERSION_STR;
extern const char *FIRMWARE_UPGRADE_VERSION_URL;
extern const char *FIRMWARE_UPGRADE_URL;

void macroResetUtils(void);

#endif /* CONFIG_UTILS_EXTERN_MACROS */

/* LED color configuration */
#define SLOW_RED (0xFF)
#define SLOW_GREEN (0x00)
#define SLOW_BLUE (0x00)

#define MEDIUM_RED (0x15)
#define MEDIUM_GREEN (0x09)
#define MEDIUM_BLUE (0x00)

#define FAST_RED (0x00)
#define FAST_GREEN (0x00)
#define FAST_BLUE (0x10)

/* Testing configuration helpers */

#ifndef CONFIG_DISABLE_TESTING_FEATURES
/* static when not testing */
#define STATIC_IF_NOT_TEST  
#else
/* not static when testing */
#define STATIC_IF_NOT_TEST static
#endif

#endif /* UTILITIES_H_ */