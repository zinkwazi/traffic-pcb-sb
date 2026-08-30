set(srcs "")
set(include_dirs "include")

list(APPEND srcs "src/led_matrix.c")
list(APPEND srcs "src/led_registers.c")
list(APPEND srcs "src/V1_0_led_registers.c")
list(APPEND srcs "src/V2_0_led_registers.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES common
                                app_errors
                                esp_event
                                driver
                                esp_adc
                                esp_driver_i2c)