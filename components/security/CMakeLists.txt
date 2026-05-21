idf_component_register(SRCS "secure_envelope.c"
                    INCLUDE_DIRS "include"
                    REQUIRES system_config
                    PRIV_REQUIRES mbedtls)
