cmake_minimum_required(VERSION 3.14)
project(nuperf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(NUPERF_BUILD_TESTS "Build tests" OFF)
option(NUPERF_BUILD_FUZZERS "Build fuzzers" OFF)
option(NUPERF_BUILD_LUA "Build Lua bindings" OFF)
option(NUPERF_BUILD_SWIG_BINDINGS "Build SWIG bindings" OFF)
option(NUPERF_BUILD_EXAMPLES "Build examples" OFF)
option(NUPERF_BUILD_CLI "Build CLI application" ON)

# Core library sources
set(NUPERF_CORE_SOURCES
    src/core/api.cpp
    src/core/builder.cpp
    src/core/keyset.cpp
    src/util/hash.cpp
    src/util/io.cpp
)

# Plugin sources
set(NUPERF_PLUGIN_SOURCES
    src/plugins/stddef.cpp
    src/plugins/stdmeth.cpp
)

# Create main library
add_library(nuperf ${NUPERF_CORE_SOURCES} ${NUPERF_PLUGIN_SOURCES})

target_include_directories(nuperf
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(nuperf PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -pedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# CLI application
if(NUPERF_BUILD_CLI)
    add_executable(nuperf-cli src/main.cpp)
    target_link_libraries(nuperf-cli PRIVATE nuperf)
    set_target_properties(nuperf-cli PROPERTIES OUTPUT_NAME nuperf)
endif()

# Lua bindings
if(NUPERF_BUILD_LUA)
    find_package(Lua REQUIRED)
    
    add_library(lnuperf MODULE
        src/lua/lnuperf.cpp
        src/lua/lua-bindings.cpp
    )
    
    target_link_libraries(lnuperf PRIVATE nuperf ${LUA_LIBRARIES})
    target_include_directories(lnuperf PRIVATE ${LUA_INCLUDE_DIR})
    
    # Use vendored sol2 if available
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sol2/include)
        target_include_directories(lnuperf PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sol2/include
        )
    endif()
    
    set_target_properties(lnuperf PROPERTIES
        PREFIX ""
        OUTPUT_NAME "lnuperf"
    )
endif()

# Tests
if(NUPERF_BUILD_TESTS)
    enable_testing()
    
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Catch2)
        add_subdirectory(third_party/Catch2)
    else()
        find_package(Catch2 REQUIRED)
    endif()
    
    file(GLOB TEST_SOURCES tests/*.cpp)
    if(TEST_SOURCES)
        add_executable(nuperf-tests ${TEST_SOURCES})
        target_link_libraries(nuperf-tests PRIVATE nuperf Catch2::Catch2)
        add_test(NAME nuperf-tests COMMAND nuperf-tests)
    endif()
endif()

# Examples
if(NUPERF_BUILD_EXAMPLES)
    file(GLOB EXAMPLE_SOURCES examples/*.c examples/*.cpp)
    foreach(example_src ${EXAMPLE_SOURCES})
        get_filename_component(example_name ${example_src} NAME_WE)
        add_executable(${example_name} ${example_src})
        target_link_libraries(${example_name} PRIVATE nuperf)
    endforeach()
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/nuperf.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/nuperf.pc
    @ONLY
)

# Installation
install(TARGETS nuperf
    EXPORT nuperf-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/nuperf
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/nuperf.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

if(NUPERF_BUILD_CLI)
    install(TARGETS nuperf-cli
        RUNTIME DESTINATION bin
    )
endif()

