idf_component_register(SRCS "src/ota.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                api_connect
                                app_errors
                                indicators
                                esp_wifi
                                esp_http_client
                                esp-tls
                                esp_https_ota)