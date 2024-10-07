idf_component_register(
    SRCS
        "text.c"
        "filesystem.c"
        "graphics.c"
        "weather.c"
        "esp32-weather-display.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_http_client
        json
        espressif__esp_lcd_touch
        esp_event
        esp_netif
        georgik__sdl
        esp_wifi_remote
)

nvs_create_partition_image(nvs ../nvs.csv FLASH_IN_PROJECT)
