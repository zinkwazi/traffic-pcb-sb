idf_component_register(SRCS "src/main.c"
                            "src/initialize.c"
                       INCLUDE_DIRS "include"
                       REQUIRES common
                                led_matrix
                                actions
                                animations
                                api_connect
                                app_errors
                                app_nvs
                                ota
                                routines
                                refresh
                                indicators
                                esp_driver_gpio
                                esp_driver_uart
                                esp_driver_usb_serial_jtag
                                esp_netif
                                esp-tls
                                vfs
                                esp_wifi
                                nvs_flash
                                esp_timer)


