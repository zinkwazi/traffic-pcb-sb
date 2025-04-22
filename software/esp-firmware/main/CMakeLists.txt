
if ("$ENV{executable}" STREQUAL "main")
    set(srcs "src/main.c"
             "src/initialize.c")
    set(includes "include")
elseif ("$ENV{executable}" STREQUAL "test_ota")
    set(srcs "src/tests/test_ota_main.c"
             "src/initialize.c")
    set(includes "include")
elseif ("$ENV{executable}" STREQUAL "test_nonmock")
    set(srcs "src/tests/test_nonmock_main.c"
             "src/initialize.c")
    set(includes "include")
elseif ("$ENV{executable}" STREQUAL "test_manual")
    set(srcs "src/tests/test_manual_main.c"
             "src/initialize.c")
    set(includes "include")
else()
    message(FATAL_ERROR "Value of executable environment variable \"$ENV{executable}\" is not known!")
endif()

set(requires unity
            common
            test_manual
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

idf_component_register(SRCS ${srcs}
                        INCLUDE_DIRS ${includes}
                        REQUIRES ${requires})