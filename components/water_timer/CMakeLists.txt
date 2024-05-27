idf_component_register(SRCS "water_timer.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_timer driver http_server esp-tls lwip esp_netif
                    )
