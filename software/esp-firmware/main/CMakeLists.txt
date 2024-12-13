idf_component_register(SRCS "utilities.c" "wifi.c" "worker.c" "led_locations.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES DotsMatrix
                             TomTom
                             esp_timer
                             esp_wifi
                             esp_http_client
                             nvs_flash
                             esp-tls
                             esp_https_ota
                             esp_event)


