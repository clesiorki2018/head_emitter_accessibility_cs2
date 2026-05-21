idf_component_register(SRCS "app_controller.c"
                    INCLUDE_DIRS "include"
                    REQUIRES input input_protocol mpr121 transport_espnow
                    PRIV_REQUIRES log freertos)
