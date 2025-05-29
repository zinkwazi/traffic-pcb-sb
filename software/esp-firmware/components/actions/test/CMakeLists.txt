set(resources "")

list(APPEND resources "resources/queryOTA_version1.json")

idf_component_register(SRC_DIRS "tests"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES ${resources}
                       REQUIRES unity
                                actions
                                common
                                ota
                                esp_http_client_wrapper)