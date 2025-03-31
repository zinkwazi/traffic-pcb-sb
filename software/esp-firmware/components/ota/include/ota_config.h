/**
 * ota_config.h
 * 
 * Contains configuration macros for over-the-air updates.
 */

#ifndef OTA_CONFIG_H_3_27_25
#define OTA_CONFIG_H_3_27_25

/*******************************************/
/*       MULTIPLE DEFINITION GUARDS        */
/*******************************************/

#ifdef RETRY_CONNECT_OTA_AVAILABLE
#error "Multiple definitions of ota_config.h RETRY_CONNECT_OTA_AVAILABLE macro!"
#endif /* RETRY_CONNECT_OTA_AVAILABLE */

#ifdef OTA_RECV_BUF_SIZE
#error "Multiple definitions of ota_config.h OTA_RECV_BUF_SIZE macro!"
#endif /* OTA_RECV_BUF_SIZE */

#ifdef HARDWARE_VERSION_KEY
#error "Multiple definitions of ota_config.h HARDWARE_VERSION_KEY macro!"
#endif /* HARDWARE_VERSION_KEY */

#ifdef HARDWARE_REVISION_KEY
#error "Multiple definitions of ota_config.h HARDWARE_REVISION_KEY macro!"
#endif /* HARDWARE_REVISION_KEY */

#ifdef FIRMWARE_MAJOR_KEY
#error "Multiple definitions of ota_config.h FIRMWARE_MAJOR_KEY macro!"
#endif /* FIRMWARE_MAJOR_KEY */

#ifdef FIRMWARE_MINOR_KEY
#error "Multiple definitions of ota_config.h FIRMWARE_MINOR_KEY macro!"
#endif /* FIRMWARE_MINOR_KEY */

#ifdef FIRMWARE_PATCH_KEY
#error "Multiple definitions of ota_config.h FIRMWARE_PATCH_KEY macro!"
#endif /* FIRMWARE_PATCH_KEY */

/*******************************************/
/*              DEFINITIONS                */
/*******************************************/

#define RETRY_CONNECT_OTA_AVAILABLE (5)
#define OTA_RECV_BUF_SIZE (128)

#define HARDWARE_VERSION_KEY "hardware_version"
#define HARDWARE_REVISION_KEY "hardware_revision"
#define FIRMWARE_MAJOR_KEY "firmware_major_version"
#define FIRMWARE_MINOR_KEY "firmware_minor_version"
#define FIRMWARE_PATCH_KEY "firmware_patch_version"

#endif /* OTA_CONFIG_H_3_27_25 */