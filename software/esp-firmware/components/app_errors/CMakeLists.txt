idf_component_register(SRCS "src/app_errors.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                esp_timer
                                driver)