/**
 * ota_config.h
 * 
 * Contains macro configuration options for ota.c, with the option to
 * use extern variables instead of macros by defining CONFIG_EXTERN_REFRESH_CONFIG.
 * This replacement is useful for testing with different values of configuration
 * macros while maintaining the benefits of static configuration options.
 */

#ifndef OTA_CONFIG_H_3_27_25
#define OTA_CONFIG_H_3_27_25

/**
 * Default definitions of macros, which add a layer of indirection allowing
 * extern macro replacements to be set to the value that the macro would be.
 */

#define DEF_RETRY_CONNECT_OTA_AVAILABLE (5)
#define DEF_OTA_RECV_BUF_SIZE (128)

#define DEF_OTA_HARDWARE_VERSION CONFIG_HARDWARE_VERSION
#define DEF_OTA_REVISION_VERSION CONFIG_HARDWARE_REVISION
#define DEF_OTA_MAJOR_VERSION CONFIG_FIRMWARE_MAJOR_VERSION
#define DEF_OTA_MINOR_VERSION CONFIG_FIRMWARE_MINOR_VERSION
#define DEF_OTA_PATCH_VERSION CONFIG_FIRMWARE_PATCH_VERSION

#define DEF_HARDWARE_VERSION_KEY "hardware_version"
#define DEF_HARDWARE_REVISION_KEY "hardware_revision"
#define DEF_FIRMWARE_MAJOR_KEY "firmware_major_version"
#define DEF_FIRMWARE_MINOR_KEY "firmware_minor_version"
#define DEF_FIRMWARE_PATCH_KEY "firmware_patch_version"

#ifndef CONFIG_OTA_EXTERN_MACROS

/**
 * Macro configuration options, set by the default macros above.
 */

#define RETRY_CONNECT_OTA_AVAILABLE DEF_RETRY_CONNECT_OTA_AVAILABLE
#define OTA_RECV_BUF_SIZE DEF_OTA_RECV_BUF_SIZE

#define OTA_HARDWARE_VERSION DEF_OTA_HARDWARE_VERSION
#define OTA_REVISION_VERSION DEF_OTA_REVISION_VERSION
#define OTA_MAJOR_VERSION DEF_OTA_MAJOR_VERSION
#define OTA_MINOR_VERSION DEF_OTA_MINOR_VERSION
#define OTA_PATCH_VERSION DEF_OTA_PATCH_VERSION

#define HARDWARE_VERSION_KEY DEF_HARDWARE_VERSION_KEY
#define HARDWARE_REVISION_KEY DEF_HARDWARE_REVISION_KEY
#define FIRMWARE_MAJOR_KEY DEF_FIRMWARE_MAJOR_KEY
#define FIRMWARE_MINOR_KEY DEF_FIRMWARE_MINOR_KEY
#define FIRMWARE_PATCH_KEY DEF_FIRMWARE_PATCH_KEY

#else /* CONFIG_OTA_EXTERN_MACROS */

/**
 * Extern definitions of configuration options, which replace macros to allow
 * testing with multiple values of macros.
 */

#include <stdint.h>

extern uint32_t RETRY_CONNECT_OTA_AVAILABLE;
extern uint32_t OTA_RECV_BUF_SIZE;

extern uint32_t OTA_HARDWARE_VERSION;
extern uint32_t OTA_REVISION_VERSION;
extern uint32_t OTA_MAJOR_VERSION;
extern uint32_t OTA_MINOR_VERSION;
extern uint32_t OTA_PATCH_VERSION;

extern const char *HARDWARE_VERSION_KEY;
extern const char *HARDWARE_REVISION_KEY;
extern const char *FIRMWARE_MAJOR_KEY;
extern const char *FIRMWARE_MINOR_KEY;
extern const char *FIRMWARE_PATCH_KEY;

void macroResetOTAConfig(void);

#endif /* CONFIG_OTA_EXTERN_MACROS */

#endif /* OTA_CONFIG_H_3_27_25 */