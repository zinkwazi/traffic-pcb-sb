set(srcs "")
set(includes "include")

if (CONFIG_MOCK_HTTP_WRAP)
    list(APPEND srcs "mocks/Mockhttp_wrap.c")
    list(APPEND includes "mocks")
else ()
    list(APPEND srcs "src/http_wrap.c")
endif ()

if (CONFIG_UTILS_EXTERN_MACROS)
    list(APPEND srcs "src/extern_utilities.c")
endif ()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${includes}
                       REQUIRES nvs_flash
                                app_errors
                                esp_driver_gpio
                                esp_http_client
                                cmock)