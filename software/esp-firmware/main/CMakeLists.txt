idf_component_register(SRCS "led_locations.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES DotsMatrix
                             TomTom
                             esp_timer
                             esp_wifi
                             esp_http_client
                             nvs_flash
                             esp-tls)


