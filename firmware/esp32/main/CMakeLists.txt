idf_component_register(
    SRCS "main.c" "gateway_verify.c"
    INCLUDE_DIRS "." "../components/ota_protocol"
    REQUIRES esp_wifi esp_http_client mbedtls driver nvs_flash log freertos esp_netif esp_event 
    esp_rom esp_system esp_timer
)
