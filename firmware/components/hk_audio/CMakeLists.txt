# hk_audio.c, hk_limiter.c, hk_biquad.c, hk_profile.c, hk_dsp.c and hk_eq.c are
# pure C and are compiled unchanged by firmware/test on a machine that has never
# seen ESP-IDF. hk_audio_hw.c is the opposite: it is the board layer that drives
# the two mute GPIOs, and it is the only file here that needs a driver. Keeping
# both in one component keeps the sequence and the pins it moves in one place;
# the host tests name the pure files individually, so this addition does not
# reach them.
set(HK_AUDIO_SRCS
    "hk_audio.c" "hk_audio_hw.c" "hk_limiter.c" "hk_biquad.c" "hk_profile.c"
    "hk_dsp.c" "hk_eq.c")

# hk_tone.c is a bench diagnostic, not a product feature, so the SOURCE is left
# out of an image that did not ask for it -- the same arrangement hk_airplay
# uses for the vendored receiver.
#
# The requirement below is NOT conditional, and the asymmetry is deliberate
# rather than an oversight: ESP-IDF resolves the component dependency graph in
# an earlier pass than the one that builds the sources, and CONFIG_ values are
# not reliably set during it. A `PRIV_REQUIRES` inside this `if()` is silently
# dropped and the build then fails on a missing driver/i2s_std.h -- which is
# exactly what it did on the first attempt here. Naming it unconditionally
# costs nothing in the image: the linker discards the sections nothing calls.
if(CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY)
    list(APPEND HK_AUDIO_SRCS "hk_tone.c")
endif()

# hk_settings is a public REQUIRES rather than a private one because hk_eq.h
# exposes an hk_setting_def_t table, so anything that includes hk_eq.h needs
# hk_settings.h too. The dependency is one-way and shallow: hk_settings is a
# leaf that knows nothing about audio, so this cannot make a cycle.
idf_component_register(
    SRCS ${HK_AUDIO_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES esp_common hk_pins hk_settings esp_driver_gpio esp_timer
    PRIV_REQUIRES esp_driver_i2s
)
