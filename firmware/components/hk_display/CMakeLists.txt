# REQUIRES is unconditional on purpose: a component's CMakeLists is processed
# once before CONFIG_ symbols exist, so a requirement behind an if() is invisible
# in that pass and the failure surfaces as missing system headers elsewhere.
idf_component_register(SRCS "hk_display.c" "hk_draw.c" "hk_gfx.c"
                            "hk_font.c" "hk_font_data.c"
                            "hk_icons.c" "hk_icons_data.c"
                            "hk_sky.c" "hk_qr.c" "hk_screen.c"
                       INCLUDE_DIRS "include"
                       REQUIRES hk_pins hk_led hk_ui hk_identity hk_storage
                                esp_lcd esp_driver_spi esp_driver_gpio esp_timer esp_hw_support)
