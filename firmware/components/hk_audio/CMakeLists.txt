idf_component_register(
    SRCS "hk_audio.c" "hk_limiter.c" "hk_biquad.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
)
