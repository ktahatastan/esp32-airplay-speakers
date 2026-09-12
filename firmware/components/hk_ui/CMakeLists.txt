idf_component_register(SRCS "hk_ui.c"
                       INCLUDE_DIRS "include"
                       REQUIRES hk_button hk_led hk_pins
                                esp_driver_gpio esp_driver_ledc esp_timer)
