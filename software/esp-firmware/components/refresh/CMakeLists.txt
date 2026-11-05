
set(srcs "src/refresh.c")
list(APPEND srcs "src/refresh_task.c")
list(APPEND srcs "src/traffic_data.c")
list(APPEND srcs "src/refresh_fsm.c")

set(include_dirs "include")

if (CONFIG_EXTERN_REFRESH_CONFIG)
    list(APPEND srcs "src/extern_refresh_config.c")
endif ()

list(APPEND srcs "src/refresh_utilities.c")


set (requires "common")
list(APPEND requires "animations")
list(APPEND requires "api_connect")
list(APPEND requires "app_errors")
list(APPEND requires "app_nvs")
list(APPEND requires "led_matrix")
list(APPEND requires "indicators")
list(APPEND requires "input")
list(APPEND requires "esp_http_client")
list(APPEND requires "esp_http_client_wrapper")
list(APPEND requires "mbedtls")

if (CONFIG_TEST_REFRESH)
    set_source_files_properties(${srcs} PROPERTIES COMPILE_FLAGS --coverage)
    list(APPEND requires "esp_gcov")
endif ()


idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES common
                                animations
                                api_connect
                                app_errors
                                app_nvs
                                led_matrix
                                indicators
                                input
                                esp_http_client
                                esp_http_client_wrapper
                                mbedtls)