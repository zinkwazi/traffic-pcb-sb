/**
 * refresh_config.h
 * 
 * Contains macro configuration options for refresh.c, with the option to
 * use extern variables instead of macros by defining CONFIG_EXTERN_REFRESH_CONFIG.
 * This replacement is useful for testing with different values of configuration
 * macros while maintaining the benefits of static configuration options.
 */

#include "esp_http_client.h"
#include "sdkconfig.h"

/**
 * Default definitions of macros, which add a layer of indirection allowing
 * extern macro replacements to be set to the value that the macro would be.
 */

#define DEF_URL_DATA_FILE_TYPE ".csv"

#define DEF_URL_DATA_CURRENT_NORTH CONFIG_DATA_SERVER "/current_data/data_north_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define DEF_URL_DATA_CURRENT_SOUTH CONFIG_DATA_SERVER "/current_data/data_south_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define DEF_URL_DATA_TYPICAL_NORTH CONFIG_DATA_SERVER "/current_data/typical_north_" SERVER_VERSION_STR URL_DATA_FILE_TYPE
#define DEF_URL_DATA_TYPICAL_SOUTH CONFIG_DATA_SERVER "/current_data/typical_south_" SERVER_VERSION_STR URL_DATA_FILE_TYPE

#define DEF_API_METHOD HTTP_METHOD_GET
#define DEF_API_AUTH_TYPE HTTP_AUTH_TYPE_NONE

#define DEF_API_RETRY_CONN_NUM 5

#define DEF_MATRIX_RETRY_NUM 15

#define DEF_DEFAULT_SCALE (0xFF)

#ifdef CONFIG_SUPPORT_STROBING
#define DEF_STROBE_LOW_SCALE (0x20)
#define DEF_STROBE_STEP_HIGH (10)
#define DEF_STROBE_STEP_LOW (10)
#endif /* CONFIG_SUPPORT_STROBING */

#ifndef CONFIG_EXTERN_REFRESH_CONFIG

/**
 * Macro configuration options, set by the default macros above.
 */

#define URL_DATA_FILE_TYPE DEF_URL_DATA_FILE_TYPE

#define URL_DATA_CURRENT_NORTH DEF_URL_DATA_CURRENT_NORTH
#define URL_DATA_CURRENT_SOUTH DEF_URL_DATA_CURRENT_SOUTH
#define URL_DATA_TYPICAL_NORTH DEF_URL_DATA_TYPICAL_NORTH
#define URL_DATA_TYPICAL_SOUTH DEF_URL_DATA_TYPICAL_SOUTH

/* TomTom HTTPS configuration */
#define API_METHOD DEF_API_METHOD
#define API_AUTH_TYPE DEF_API_AUTH_TYPE

#define API_RETRY_CONN_NUM DEF_API_RETRY_CONN_NUM

#define MATRIX_RETRY_NUM DEF_MATRIX_RETRY_NUM

#define DEFAULT_SCALE DEF_DEFAULT_SCALE

#ifdef CONFIG_SUPPORT_STROBING
#define STROBE_LOW_SCALE DEF_STROBE_LOW_SCALE
#define STROBE_STEP_HIGH DEF_STROBE_STEP_HIGH
#define STROBE_STEP_LOW DEF_STROBE_STEP_LOW
#endif /* CONFIG_SUPPORT_STROBING */

#else /* CONFIG_EXTERN_REFRESH_CONFIG */

/**
 * Extern definitions of configuration options, which replace macros to allow
 * testing with multiple values of macros.
 */

#include <stdint.h>

extern const char *URL_DATA_FILE_TYPE;

extern const char *URL_DATA_CURRENT_NORTH;
extern const char *URL_DATA_CURRENT_SOUTH;
extern const char *URL_DATA_TYPICAL_NORTH;
extern const char *URL_DATA_TYPICAL_SOUTH;

/* TomTom HTTPS configuration */
extern esp_http_client_method_t API_METHOD;
extern esp_http_client_auth_type_t API_AUTH_TYPE;

extern uint32_t API_RETRY_CONN_NUM;

extern uint32_t MATRIX_RETRY_NUM;

extern uint8_t DEFAULT_SCALE;

#ifdef CONFIG_SUPPORT_STROBING
extern uint8_t STROBE_LOW_SCALE;
extern uint8_t STROBE_STEP_HIGH;
extern uint8_t STROBE_STEP_LOW;
#endif /* CONFIG_SUPPORT_STROBING */

void macroResetRefreshConfig(void);

#endif /* CONFIG_EXTERN_REFRESH_CONFIG */