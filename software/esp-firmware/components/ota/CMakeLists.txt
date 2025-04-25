set(srcs "src/ota.c")

if(CONFIG_OTA_EXTERN_MACROS)
    list(APPEND srcs "src/extern_ota_config.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                api_connect
                                app_errors
                                indicators
                                esp_http_client
                                esp_https_ota)