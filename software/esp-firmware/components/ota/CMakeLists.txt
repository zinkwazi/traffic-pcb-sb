set(srcs "")
set(includes "include")

if (CONFIG_MOCK_OTA)
    list(APPEND srcs "mocks/Mockota.c")
    list(APPEND includes "mocks")
else ()
    list(APPEND srcs "src/ota.c")
endif ()

if (CONFIG_OTA_EXTERN_MACROS)
    list(APPEND srcs "src/extern_ota_config.c")
endif ()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${includes}
                       REQUIRES common
                                circular_buffer
                                api_connect
                                app_errors
                                indicators
                                esp_http_client
                                esp_https_ota)