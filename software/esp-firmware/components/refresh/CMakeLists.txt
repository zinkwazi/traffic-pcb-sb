idf_component_register(SRCS "src/refresh.c"
                               "src/traffic_data.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                animations
                                api_connect
                                app_errors
                                app_nvs
                                led_matrix
                                esp_http_client
                                mbedtls)