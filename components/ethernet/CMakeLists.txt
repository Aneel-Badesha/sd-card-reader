idf_component_register(
    SRCS "ethernet.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_eth esp_netif esp_event sdcard
)
