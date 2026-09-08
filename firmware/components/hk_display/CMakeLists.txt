# REQUIRES is unconditional on purpose: a component's CMakeLists is processed
# once before CONFIG_ symbols exist, so a requirement behind an if() is invisible
# in that pass and the failure surfaces as missing system headers elsewhere.
# hk_draw.c is unconditional and everything else is not. It touches no
# peripheral -- it is arithmetic over a buffer -- so it costs nothing to keep,
# and keeping one source stops the component from degenerating into an
# INTERFACE library when the panel is compiled out, which changes how its
# requirements propagate.
set(HK_DISPLAY_SRCS "hk_draw.c")
if(CONFIG_HK_DISPLAY)
    list(APPEND HK_DISPLAY_SRCS
         "hk_display.c" "hk_gfx.c" "hk_font.c" "hk_font_data.c"
         "hk_icons.c" "hk_icons_data.c" "hk_sky.c" "hk_qr.c" "hk_screen.c")
endif()

idf_component_register(SRCS ${HK_DISPLAY_SRCS}
                       INCLUDE_DIRS "include"
                       REQUIRES hk_pins hk_led hk_ui hk_identity hk_provision hk_storage
                                esp_lcd esp_driver_spi esp_driver_gpio esp_timer esp_hw_support)
