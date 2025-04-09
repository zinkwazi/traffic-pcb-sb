idf_component_register(SRCS "src/actions.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                app_errors
                                indicators
                                ota
                                esp_netif
                                lwip)