idf_component_register(SRC_DIRS "src"
                                 "fake"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                app_errors
                                driver
                                esp_timer)
