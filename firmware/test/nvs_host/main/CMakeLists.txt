# hk_storage.c and hk_schema.c are compiled here, not linked as components.
# That keeps the dependency graph to what nvs_flash and esp_partition need, and
# it is the real source either way — the point of this test is that it exercises
# the shipped code rather than a copy of its logic.
idf_component_register(
    SRCS "test_storage_wall.c"
         "${CMAKE_CURRENT_LIST_DIR}/../../../components/hk_storage/hk_storage.c"
         "${CMAKE_CURRENT_LIST_DIR}/../../../components/hk_schema/hk_schema.c"
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../../components/hk_storage/include"
                 "${CMAKE_CURRENT_LIST_DIR}/../../../components/hk_schema/include"
    REQUIRES nvs_flash esp_partition)
