idf_component_register(SRCS "wifi_connect.c" "main.c"
                    PRIV_REQUIRES esp_wifi nvs_flash
                    INCLUDE_DIRS ".")
