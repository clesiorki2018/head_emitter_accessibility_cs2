# SPDX-License-Identifier: Apache-2.0
idf_component_register(SRCS "sequence_store.c"
                    INCLUDE_DIRS "include"
                    REQUIRES system_config nvs_flash
                    PRIV_REQUIRES log)
