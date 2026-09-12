# Merzarkabul Airplay Speakers firmware, ESP32-S3 N16R8 (ADR-0010).
#
#   . $IDF_PATH/export.sh
#   idf.py -C firmware build
#
# PROJECT_VER is taken from firmware/version.txt by ESP-IDF. It must stay strict
# SemVer: the OTA client compares it numerically, and the release pipeline checks
# it against the Git tag (ADR-0008).
#
# There is a second, non-shipping target for the N8R2 bring-up devkit. See
# sdkconfig.devkit for the command line. The guards at the bottom of this file are
# what keep the two from being mixed up; read them before changing either config.
# The last guard is about the bench fragments (sdkconfig.bench, sdkconfig.sweep):
# they build for the product board, so nothing above tells them apart from a
# release, and the guard is what refuses to sign one.
cmake_minimum_required(VERSION 3.16)

# The second entry holds the two upstream components the vendored AirPlay stack
# builds on (ADR-0007). They are registered from where they were copied rather
# than moved next to ours, so `vendor/` stays byte-identical to upstream and the
# resampler keeps the compile options upstream gives it.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components"
    "${CMAKE_CURRENT_LIST_DIR}/components/hk_airplay/vendor/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(merzarkabul-airplay-speakers)

# --- Board configuration guards -------------------------------------------------
#
# A board variant is four settings that have to agree: flash size, PSRAM mode,
# partition table and OTA hardware revision. Three of them fail late and quietly
# when they disagree - a wrong PSRAM mode passes its own memory test, a wrong flash
# size corrupts the second OTA slot, a wrong partition table boots fine until the
# first update - so they are checked here, at configure time, where the failure is
# a message instead of a bench mystery.
#
# These are `if` statements rather than a comment saying "remember to change all
# four" because that comment has never once worked.

if(CONFIG_HK_BOARD_DEVKIT_N8R2)
    if(NOT CONFIG_ESPTOOLPY_FLASHSIZE_8MB)
        message(FATAL_ERROR
            "HK_BOARD_DEVKIT_N8R2 is set but the flash size is not 8 MB. The devkit "
            "has 8 MB; building it as 16 MB writes a wrong image header and ota_1 "
            "aliases onto ota_0. Use SDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.devkit'.")
    endif()
    if(NOT CONFIG_SPIRAM_MODE_QUAD)
        message(FATAL_ERROR
            "HK_BOARD_DEVKIT_N8R2 is set but PSRAM is not in quad mode. The devkit's "
            "2 MB PSRAM is inside the package and is quad; octal mode still passes "
            "its startup memory test and then corrupts the heap under load.")
    endif()
    if(NOT CONFIG_PARTITION_TABLE_CUSTOM_FILENAME STREQUAL "partitions-devkit.csv")
        message(FATAL_ERROR
            "HK_BOARD_DEVKIT_N8R2 is set but the partition table is "
            "'${CONFIG_PARTITION_TABLE_CUSTOM_FILENAME}'. The 16 MB table does not fit "
            "8 MB of flash; use partitions-devkit.csv.")
    endif()
    if(CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT)
        message(FATAL_ERROR
            "Refusing to sign a devkit build. A signed image is a publishable image, "
            "and this one is built for 8 MB flash with a different partition table. "
            "Signing belongs to sdkconfig.release, which targets the product board.")
    endif()
else()
    if(NOT CONFIG_ESPTOOLPY_FLASHSIZE_16MB)
        message(FATAL_ERROR
            "The product board is N16R8, 16 MB flash (ADR-0010), but the flash size is "
            "not 16 MB. If you meant to build the bring-up devkit, select "
            "HK_BOARD_DEVKIT_N8R2 as sdkconfig.devkit does; changing the flash size "
            "alone produces an image that claims to be a product image.")
    endif()
    if(NOT CONFIG_SPIRAM_MODE_OCT)
        message(FATAL_ERROR
            "The product board has 8 MB octal PSRAM (ADR-0010) but PSRAM is not in "
            "octal mode. See the note above about HK_BOARD_DEVKIT_N8R2.")
    endif()
endif()

# --- Bench builds are never signed ------------------------------------------------
#
# A bench fragment differs from the product by nothing the guards above can see:
# same board, same flash, same partition table, and since ADR-0022 the same DSP
# output backend. What it changes is two gate symbols -- audio permitted with no
# driver-protection profile, and a compiled-in profile nobody measured -- plus,
# in the sweep overlay, a tone written straight to I2S past the DSP. Each is a
# bool under HK_BENCH_ in main/Kconfig.projbuild. A signed image is a
# publishable image, so the combination is refused here, at configure time,
# the way the devkit is. release.yml checks the generated sdkconfig for the
# same symbols with one regex; this is the check that runs on every machine.
if(CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT)
    if(CONFIG_HK_BENCH_AUDIO_WITHOUT_PROFILE
       OR CONFIG_HK_BENCH_PROVISIONAL_PROFILE
       OR CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY
       OR CONFIG_HK_BENCH_SWEEP)
        message(FATAL_ERROR
            "Refusing to sign a bench build: this image releases the amplifiers on "
            "numbers nobody measured (sdkconfig.bench:1 says NEVER part of a release). "
            "One of CONFIG_HK_BENCH_AUDIO_WITHOUT_PROFILE, "
            "CONFIG_HK_BENCH_PROVISIONAL_PROFILE, CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY "
            "or CONFIG_HK_BENCH_SWEEP is set together with "
            "CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT. Drop the bench fragment from "
            "SDKCONFIG_DEFAULTS, or drop sdkconfig.release.")
    endif()
endif()
