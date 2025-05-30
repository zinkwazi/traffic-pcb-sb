set(resources "")
list(APPEND resources "resources/data_north_add_V1_0_5.csv")
list(APPEND resources "resources/data_north_add_V2_0_0.csv")
list(APPEND resources "resources/data_north_V1_0_3.dat")
list(APPEND resources "resources/data_north_V1_0_5.csv")
list(APPEND resources "resources/getNextResponseBlock_appendsNewline1.txt")
list(APPEND resources "resources/openServerFile_typical1.txt")
list(APPEND resources "resources/openServerFile_zeroContentLength1.txt")
list(APPEND resources "resources/readServerSpeedDataPreinit_smallFile.csv")

idf_component_register(SRC_DIRS "tests"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES ${resources}
                       REQUIRES unity
                                api_connect
                                app_errors
                                common
                                esp_wifi
                                esp_http_client
                                esp-tls
                                esp_http_client_wrapper)