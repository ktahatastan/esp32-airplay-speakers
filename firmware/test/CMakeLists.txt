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
    ${HK_COMPONENTS}/hk_pins/hk_pins.c
    ${HK_COMPONENTS}/hk_identity/hk_identity.c
    ${HK_COMPONENTS}/hk_version/hk_version.c
)

target_include_directories(hk_host_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${HK_COMPONENTS}/hk_pins/include
    ${HK_COMPONENTS}/hk_identity/include
    ${HK_COMPONENTS}/hk_version/include
)

enable_testing()
add_test(NAME hk_host_tests COMMAND hk_host_tests)
