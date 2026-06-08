# SPDX-License-Identifier: Apache-2.0
idf_component_register(SRCS "accessibility.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_common
                    PRIV_REQUIRES log)
