idf_component_register(SRCS "http_server.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi nvs_flash esp_http_server driver water_timer esp_http_client json
                    )
