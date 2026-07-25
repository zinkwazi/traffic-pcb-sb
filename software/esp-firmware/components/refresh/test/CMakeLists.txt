set(resources "")

# list(APPEND resources "resources/ledFrame1.txt")

idf_component_register(SRC_DIRS "tests"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES ${resources}
                       REQUIRES unity
                                refresh
                                led_matrix
                                common)