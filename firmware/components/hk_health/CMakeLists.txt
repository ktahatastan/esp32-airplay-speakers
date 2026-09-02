# hk_health.c is pure C and is compiled by the host test build too. Keep it
# free of ESP-IDF headers: the decision is the part worth exercising
# exhaustively, and it can only be exercised on a laptop if it stays portable.
idf_component_register(
    SRCS "hk_health.c" "hk_health_monitor.c"
    INCLUDE_DIRS "include"
    REQUIRES app_update esp_system
)
