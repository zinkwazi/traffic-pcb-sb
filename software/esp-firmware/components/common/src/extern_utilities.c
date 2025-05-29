/**
 * extern_utilities.c
 * 
 * Contains extern definitions of utilities.h macros when
 * CONFIG_UTILS_EXTERN_MACROS is defined, which is useful for testing.
 */

#include "sdkconfig.h"

#ifdef CONFIG_UTILS_EXTERN_MACROS

#include "utilities.h"

const char *HARDWARE_VERSION_STR = DEF_HARDWARE_VERSION_STR;
const char *SERVER_HARDWARE_VERSION_STR = DEF_SERVER_HARDWARE_VERSION_STR;
const char *FIRMWARE_VERSION_STR = DEF_FIRMWARE_VERSION_STR;
const char *SERVER_VERSION_STR = DEF_SERVER_VERSION_STR;
const char *FIRMWARE_UPGRADE_VERSION_URL = DEF_FIRMWARE_UPGRADE_VERSION_URL;
const char *FIRMWARE_UPGRADE_URL = DEF_FIRMWARE_UPGRADE_URL;

/**
 * @brief Resets utilities.h extern macros to their default value, which
 * is the value the macro will have when CONFIG_UTILS_EXTERN_MACROS is not
 * defined.
 */
void macroResetUtils(void)
{
    HARDWARE_VERSION_STR = DEF_HARDWARE_VERSION_STR;
    SERVER_HARDWARE_VERSION_STR = DEF_SERVER_HARDWARE_VERSION_STR;
    FIRMWARE_VERSION_STR = DEF_FIRMWARE_VERSION_STR;
    SERVER_VERSION_STR = DEF_SERVER_VERSION_STR;
    FIRMWARE_UPGRADE_VERSION_URL = DEF_FIRMWARE_UPGRADE_VERSION_URL;
    FIRMWARE_UPGRADE_URL = DEF_FIRMWARE_UPGRADE_URL;
}

#else /* CONFIG_UTILS_EXTERN_MACROS */
#error "extern_utilities.c compiled when CONFIG_UTILS_EXTERN_MACROS is undefined!"
#endif /* CONFIG_UTILS_EXTERN_MACROS */
