idf_component_register(SRCS "src/app_nvs.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                app_errors
                                indicators
                                led_matrix
                                routines
                                driver
                                nvs_flash
                                esp_timer)