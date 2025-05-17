set(srcs "src/app_nvs.c")
set(include_dirs "include")

if (CONFIG_MOCK_APP_NVS)
    set(srcs "mocks/Mockapp_nvs.c")
    list(APPEND include_dirs "mocks")
endif ()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES common
                                app_errors
                                indicators
                                led_matrix
                                routines
                                driver
                                nvs_flash
                                esp_timer
                                cmock)