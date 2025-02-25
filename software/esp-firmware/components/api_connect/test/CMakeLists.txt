idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       EMBED_FILES test_data/data_north_V1_0_5.csv
                                   test_data/data_north_V1_0_3.dat
                       REQUIRES unity
                                api_connect
                                common)