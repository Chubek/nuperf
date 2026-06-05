cmake_minimum_required(VERSION 3.16)

set(NUPERF_EXAMPLE_C_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/embedded.c
    ${CMAKE_CURRENT_SOURCE_DIR}/c/c_basic_build.c
    ${CMAKE_CURRENT_SOURCE_DIR}/c/c_emit_buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/c/c_emit_file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/c/c_list_plugins.c
    ${CMAKE_CURRENT_SOURCE_DIR}/c/c_stats.c
    ${CMAKE_CURRENT_SOURCE_DIR}/c/c_binary_keys.c
    ${CMAKE_CURRENT_SOURCE_DIR}/c/c_uint32_keys.c
    ${CMAKE_CURRENT_SOURCE_DIR}/c/c_writer_callback.c
)

foreach(example_src IN LISTS NUPERF_EXAMPLE_C_SOURCES)
    get_filename_component(example_name ${example_src} NAME_WE)
    add_executable(${example_name} ${example_src})
    target_link_libraries(${example_name} PRIVATE nuperf)
endforeach()

add_custom_target(nuperf-example-files ALL
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/basic.lua
        ${CMAKE_CURRENT_SOURCE_DIR}/months.lua
        ${CMAKE_CURRENT_SOURCE_DIR}/lua/build_from_list.lua
        ${CMAKE_CURRENT_SOURCE_DIR}/lua/months_table.lua
        ${CMAKE_CURRENT_SOURCE_DIR}/lua/stats.lua
        ${CMAKE_CURRENT_SOURCE_DIR}/lua/list_plugins.lua
        ${CMAKE_CURRENT_SOURCE_DIR}/lua/options_demo.lua
        ${CMAKE_CURRENT_SOURCE_DIR}/text/animals.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/text/colors.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/text/fruits.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/text/months.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/text/planets.txt
)
