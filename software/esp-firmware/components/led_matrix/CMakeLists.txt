idf_component_register(SRCS "src/led_matrix.c"
                            "src/V1_0_led_registers.c"
                            "src/V2_0_led_registers.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                app_errors
                                esp_event
                                driver
                                esp_adc
                                esp_driver_i2c)