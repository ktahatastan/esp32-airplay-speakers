# hk_ota.c is pure C and is also compiled by the host test build. Keep it free
# of ESP-IDF headers so that stays true.
idf_component_register(
    SRCS "hk_ota.c" "hk_ota_client.c"
    INCLUDE_DIRS "include"
    REQUIRES hk_manifest hk_gate esp_https_ota esp_http_client mbedtls json app_update
)
