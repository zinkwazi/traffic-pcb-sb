set(srcs "src/circular_buffer.c")

if (CONFIG_UTILS_EXTERN_MACROS)
    list(APPEND srcs "src/extern_utilities.c")
endif ()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash
                                app_errors
                                esp_driver_gpio
                                esp_http_client)