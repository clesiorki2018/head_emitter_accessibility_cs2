idf_component_register(SRCS "mpr121.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_i2c esp_common
                    PRIV_REQUIRES freertos)
