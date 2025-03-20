idf_component_register(SRCS "src/main.c" 
                            "src/routines.c"
                            "src/tasks.c"
                            "src/nvs_settings.c"
                            "src/refresh.c"
                            "src/initialize.c"
                    INCLUDE_DIRS "include"
                    REQUIRES common
                             app_errors  
                             led_matrix
                             animations
                             api_connect
                             indicators
                             esp_timer
                             esp_wifi
                             esp_http_client
                             esp-tls
                             esp_https_ota
                             esp_event)


