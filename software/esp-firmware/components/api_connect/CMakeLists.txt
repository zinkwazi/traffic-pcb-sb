idf_component_register(SRCS "src/api_connect.c"
                            "src/wifi.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                app_errors
                                indicators
                                esp_wifi
                                esp_http_client
                                esp-tls)