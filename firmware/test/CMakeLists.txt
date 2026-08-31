# Host-side unit tests. Deliberately independent of ESP-IDF: every module under
# test is pure C, so the logic can be checked on any machine in under a second
# and long before hardware exists.
#
#   cmake -S firmware/test -B build/host-tests
#   cmake --build build/host-tests
#   ctest --test-dir build/host-tests --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(hk_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(
    -Wall -Wextra -Werror
    -Wshadow -Wconversion -Wsign-conversion
    -Wstrict-prototypes -Wmissing-prototypes
    -Wpointer-arith -Wcast-qual
)

set(HK_COMPONENTS ${CMAKE_CURRENT_SOURCE_DIR}/../components)

add_executable(hk_host_tests
    test_main.c
    test_pins.c
    test_identity.c
    test_version.c
    test_button.c
    test_led.c
    test_provision.c
    test_schema.c
    test_manifest.c
    test_gate.c
    test_ota.c
    ${HK_COMPONENTS}/hk_pins/hk_pins.c
    ${HK_COMPONENTS}/hk_identity/hk_identity.c
    ${HK_COMPONENTS}/hk_version/hk_version.c
    ${HK_COMPONENTS}/hk_button/hk_button.c
    ${HK_COMPONENTS}/hk_led/hk_led.c
    ${HK_COMPONENTS}/hk_provision/hk_provision.c
    ${HK_COMPONENTS}/hk_schema/hk_schema.c
    ${HK_COMPONENTS}/hk_manifest/hk_manifest.c
    ${HK_COMPONENTS}/hk_gate/hk_gate.c
    ${HK_COMPONENTS}/hk_ota/hk_ota.c
)

target_include_directories(hk_host_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${HK_COMPONENTS}/hk_pins/include
    ${HK_COMPONENTS}/hk_identity/include
    ${HK_COMPONENTS}/hk_version/include
    ${HK_COMPONENTS}/hk_button/include
    ${HK_COMPONENTS}/hk_led/include
    ${HK_COMPONENTS}/hk_provision/include
    ${HK_COMPONENTS}/hk_schema/include
    ${HK_COMPONENTS}/hk_manifest/include
    ${HK_COMPONENTS}/hk_gate/include
    ${HK_COMPONENTS}/hk_ota/include
)

enable_testing()
add_test(NAME hk_host_tests COMMAND hk_host_tests)
