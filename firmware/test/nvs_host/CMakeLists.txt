cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# FreeRTOS is mocked: nothing here needs a scheduler.
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/mocks/freertos/")

# Only what this test needs. The firmware's own sources are compiled straight
# into main rather than registered as components: adding the components
# directory to EXTRA_COMPONENT_DIRS makes the build graph expand until it
# includes vfs, which wants sys/eventfd.h — a Linux-only header, so the test
# would stop running on the developer's own machine for no benefit.
set(COMPONENTS main)

# ESP-IDF's vendored sources are compiled with this project's -Werror, and
# clang raises warnings GCC does not. A host test is not a shipped image.
idf_build_set_property(COMPILE_OPTIONS "-Wno-error" APPEND)

project(hk_nvs_host)

# Build the real partition table the device uses, so the test cannot drift
# from it: the offsets under test come from the same CSV the firmware ships.
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hk-partitions.bin"
    COMMAND ${PYTHON} "$ENV{IDF_PATH}/components/partition_table/gen_esp32part.py"
            "${CMAKE_CURRENT_LIST_DIR}/../../partitions.csv"
            "${CMAKE_CURRENT_BINARY_DIR}/hk-partitions.bin"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../../partitions.csv"
    COMMENT "Generating the real partition table for the host test"
    VERBATIM)
add_custom_target(hk_partition_table ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/hk-partitions.bin")
add_dependencies(${CMAKE_PROJECT_NAME}.elf hk_partition_table)
