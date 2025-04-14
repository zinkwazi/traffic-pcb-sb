idf_component_register(SRCS "src/routines.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                animations
                                led_matrix
                                esp_timer
                                esp_driver_gpio)