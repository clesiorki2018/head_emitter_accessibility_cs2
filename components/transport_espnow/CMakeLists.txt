idf_component_register(SRCS "espnow_transport.c"
                    INCLUDE_DIRS "include"
                    REQUIRES system_config
                    PRIV_REQUIRES log esp_wifi esp_netif esp_event)
