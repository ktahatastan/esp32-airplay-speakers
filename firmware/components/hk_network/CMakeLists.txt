idf_component_register(SRCS "hk_network.c"
                       INCLUDE_DIRS "include"
                       REQUIRES hk_identity hk_portal hk_provision hk_storage hk_ui
                                esp_wifi esp_netif esp_event nvs_flash
                                wifi_provisioning)
