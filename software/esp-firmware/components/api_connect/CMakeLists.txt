idf_component_register(SRCS "src/api_connect.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                esp_wifi
                                esp_http_client)