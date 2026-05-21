idf_component_register(SRCS "app_controller.c"
                    INCLUDE_DIRS "include"
                    REQUIRES input_protocol transport_espnow
                    PRIV_REQUIRES log freertos)
