# hk_storage.c and hk_schema.c are compiled here, not linked as components.
# That keeps the dependency graph to what nvs_flash and esp_partition need, and
# it is the real source either way — the point of this test is that it exercises
# the shipped code rather than a copy of its logic.
#
# The four hk_audio files are the judge and what it needs to build a chain:
# hk_profile.c is hk_profile_load(), the one function hk_main and the output
# backend both judge a stored profile with (ADR-0022), and the other three are
# the filter, limiter and supply-budget designs it refuses on. Pure C, no
# driver, no RTOS -- the same files firmware/test compiles on a laptop. They
# are here so the wall test can do what the boot does: read the blob back
# through hk_storage_factory_get_blob(), judge it, and hand the verdict to
# hk_storage_profile_judged(). A test that wrote a blob and then declared it
# valid by hand would be proving the gate around an assertion, not around a
# profile. libm is named because the designs use tanf/expf and the linux
# target does not add it on its own.
set(HK_AUDIO_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../components/hk_audio")
idf_component_register(
    SRCS "test_storage_wall.c"
         "${CMAKE_CURRENT_LIST_DIR}/../../../components/hk_storage/hk_storage.c"
         "${CMAKE_CURRENT_LIST_DIR}/../../../components/hk_schema/hk_schema.c"
         "${HK_AUDIO_DIR}/hk_profile.c"
         "${HK_AUDIO_DIR}/hk_biquad.c"
         "${HK_AUDIO_DIR}/hk_limiter.c"
         "${HK_AUDIO_DIR}/hk_supply_limiter.c"
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../../components/hk_storage/include"
                 "${CMAKE_CURRENT_LIST_DIR}/../../../components/hk_schema/include"
                 "${HK_AUDIO_DIR}/include"
    REQUIRES nvs_flash esp_partition)
target_link_libraries(${COMPONENT_LIB} PUBLIC m)
