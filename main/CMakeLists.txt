idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES http_server water_timer data_storage
                    )