# hk_ota.c and hk_manifest_json.c are free of ESP-IDF headers and are compiled
# by the host test build too. Keep them that way: it is the only reason the
# generator, the parser and the validator can be tested against each other.
idf_component_register(
    SRCS "hk_ota.c" "hk_manifest_json.c" "hk_ota_client.c"
    INCLUDE_DIRS "include"
    REQUIRES hk_manifest hk_gate esp_https_ota esp_http_client mbedtls json app_update
)
