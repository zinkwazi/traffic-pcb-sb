idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       REQUIRES main
                                esp_event
                                driver
                                esp_driver_i2c)