idf_component_register(SRCS "src/actions.c"
                            "src/action_task.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                app_errors
                                indicators
                                led_matrix
                                refresh
                                ota
                                esp_netif
                                esp_timer
                                lwip)