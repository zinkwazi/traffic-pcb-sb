idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES manual_verifier
                             common
                             app_errors  
                             led_matrix
                             unity
                             driver)