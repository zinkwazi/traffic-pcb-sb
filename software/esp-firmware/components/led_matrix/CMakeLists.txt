idf_component_register(SRCS "src/dots_commands.c" 
                            "src/dots_matrix.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                esp_event
                                driver
                                esp_driver_i2c)