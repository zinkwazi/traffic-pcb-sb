idf_component_register(SRCS "routines.c" "tasks.c" "utilities.c" "wifi.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES DotsMatrix
                             esp_timer
                             esp_wifi
                             esp_http_client
                             nvs_flash
                             esp-tls
                             esp_https_ota
                             esp_event)


