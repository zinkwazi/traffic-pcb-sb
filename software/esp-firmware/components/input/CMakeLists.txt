idf_component_register(SRCS "src/input.c"
                            "src/input_queue.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                app_errors
                                refresh)
