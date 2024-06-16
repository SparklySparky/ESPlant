idf_component_register(SRCS "data_storage.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi nvs_flash esp_http_server driver water_timer esp_http_client json esp-tls lwip esp_netif
                    )
