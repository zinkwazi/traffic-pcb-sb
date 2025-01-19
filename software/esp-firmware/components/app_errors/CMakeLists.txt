idf_component_register(SRCS "src/error.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                esp_timer
                                driver)