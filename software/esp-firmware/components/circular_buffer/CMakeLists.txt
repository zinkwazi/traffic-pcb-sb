set(srcs "src/circular_buffer.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES app_errors
                                common
                                esp_http_client
                                esp_http_client_wrapper)