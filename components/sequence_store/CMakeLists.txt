idf_component_register(SRCS "sequence_store.c"
                    INCLUDE_DIRS "include"
                    REQUIRES system_config nvs_flash
                    PRIV_REQUIRES log)
