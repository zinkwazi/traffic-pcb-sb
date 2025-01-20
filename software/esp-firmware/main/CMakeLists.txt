idf_component_register(SRCS "src/main.c" 
                            "src/routines.c"
                            "src/tasks.c" 
                            "src/utilities.c" 
                            "src/wifi.c"
                            "src/nvs_settings.c"
                    INCLUDE_DIRS "include"
                    REQUIRES common
                             app_errors  
                             led_matrix
                             esp_timer
                             esp_wifi
                             esp_http_client
                             nvs_flash
                             esp-tls
                             esp_https_ota
                             esp_event)


