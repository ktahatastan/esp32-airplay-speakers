# REQUIRES is listed unconditionally on purpose: a component's CMakeLists is
# processed once before CONFIG_ symbols exist, and a requirement hidden behind
# an if() is invisible in that pass. The failure then surfaces as missing
# system headers somewhere else entirely.
idf_component_register(SRCS "hk_portal.c" "hk_portal_dns.c" "hk_portal_form.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_server esp_wifi esp_netif lwip
                                wifi_provisioning hk_identity)
