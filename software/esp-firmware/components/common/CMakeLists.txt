idf_component_register(SRCS "src/circular_buffer.c"
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash
                                esp_http_client)