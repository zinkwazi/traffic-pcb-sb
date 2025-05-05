set(srcs "src/circular_buffer.c" "src/http_wrap.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES app_errors
                                common
                                esp_http_client)