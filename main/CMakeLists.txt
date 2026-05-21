idf_component_register(SRCS "head_emitter.c"
                    INCLUDE_DIRS "."
                    REQUIRES app nvs_flash
                    PRIV_REQUIRES log)
