# hk_audio.c, hk_limiter.c, hk_biquad.c and hk_profile.c are pure C and are
# compiled unchanged by firmware/test on a machine that has never seen ESP-IDF.
# hk_audio_hw.c is the opposite: it is the board layer that drives the two mute
# GPIOs, and it is the only file here that needs a driver. Keeping both in one
# component keeps the sequence and the pins it moves in one place; the host
# tests name the pure files individually, so this addition does not reach them.
idf_component_register(
    SRCS "hk_audio.c" "hk_audio_hw.c" "hk_limiter.c" "hk_biquad.c" "hk_profile.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common hk_pins esp_driver_gpio esp_timer
)
