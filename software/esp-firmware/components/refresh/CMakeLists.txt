
set(srcs "src/refresh.c")
set(include_dirs "include")

if (CONFIG_MOCK_REFRESH)
    set(srcs "mocks/Mockrefresh.c")
    list(APPEND include_dirs "mocks")
endif ()

# non-mockable source files
list(APPEND srcs "src/traffic_data.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES common
                                animations
                                api_connect
                                app_errors
                                app_nvs
                                led_matrix
                                esp_http_client
                                mbedtls
                                cmock)