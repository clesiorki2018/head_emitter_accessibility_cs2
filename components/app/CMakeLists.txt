# SPDX-License-Identifier: Apache-2.0
idf_component_register(SRCS "app_controller.c"
                    INCLUDE_DIRS "include"
                    REQUIRES accessibility input input_protocol mpr121 security sequence_store transport_espnow
                    PRIV_REQUIRES log freertos)
