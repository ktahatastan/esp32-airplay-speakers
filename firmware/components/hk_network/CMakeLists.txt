# hk_storage is deliberately absent: since ADR-0023 nothing in this component
# reads factory_cal. Setup needs no per-device secret, so the network layer has
# no business behind the PRD-008 wall at all.
idf_component_register(SRCS "hk_network.c"
                       INCLUDE_DIRS "include"
                       REQUIRES hk_identity hk_portal hk_provision
                                esp_wifi esp_netif esp_event nvs_flash
                                wifi_provisioning)
