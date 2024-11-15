idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       EMBED_FILES tomtom_ca_cert.pem
                       REQUIRES esp_event
                                esp_wifi
                                esp_http_client
                                mbedtls
                                esp-tls)