idf_component_register(SRCS "src/animations.c"
                            "src/strobe_task.c"
                            "src/strobe.c"
                            "src/V1_0_led_coordinates.c"
                            "src/V2_0_led_coordinates.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                app_errors
                                led_matrix)