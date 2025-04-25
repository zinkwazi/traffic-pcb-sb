set(srcs "src/api_connect")
set(includes "include")

if (CONFIG_MOCK_API_CONNECT)
     set(srcs "mocks/Mockapi_connect.c")
     list(APPEND includes "mocks")
endif ()

list(APPEND srcs "src/wifi.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${includes}
                       REQUIRES common
                                app_errors
                                indicators
                                esp_wifi
                                esp_http_client
                                esp-tls)