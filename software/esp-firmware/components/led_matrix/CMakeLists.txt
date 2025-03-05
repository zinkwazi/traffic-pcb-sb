idf_component_register(SRCS "src/V1_0_led_matrix.c"
                            "src/V2_0_led_matrix.c"
                            "src/V1_0_led_registers.c"
                            "src/V2_0_led_registers.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                esp_event
                                driver
                                esp_driver_i2c)