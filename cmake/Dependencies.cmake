# Vendored dependency wiring for NuPERF.
# Keep this minimal and target-based.

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/CLI11/include")
    add_library(nuperf_cli11 INTERFACE)
    target_include_directories(nuperf_cli11 INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/CLI11/include")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Klyspec")
    add_library(nuperf_klyspec INTERFACE)
    target_include_directories(nuperf_klyspec INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Klyspec")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dynalo/include")
    add_library(nuperf_dynalo INTERFACE)
    target_include_directories(nuperf_dynalo INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dynalo/include")
    if(UNIX)
        target_link_libraries(nuperf_dynalo INTERFACE dl)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sol2/include")
    add_library(nuperf_sol2 INTERFACE)
    target_include_directories(nuperf_sol2 INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sol2/include")
endif()

set(_NUPERF_SERDETK_DIR "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SerdeTk/SerdeTk.hpp")
    set(_NUPERF_SERDETK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SerdeTk")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Klyspec/SerdeTk/SerdeTk.hpp")
    set(_NUPERF_SERDETK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Klyspec/SerdeTk")
endif()

if(_NUPERF_SERDETK_DIR)
    add_library(nuperf_serdetk INTERFACE)
    target_include_directories(nuperf_serdetk INTERFACE
        $<BUILD_INTERFACE:${_NUPERF_SERDETK_DIR}>
        $<INSTALL_INTERFACE:include>)
    install(TARGETS nuperf_serdetk
        EXPORT nuperfTargets)

    install(
        DIRECTORY ${_NUPERF_SERDETK_DIR}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
    )
else()
    message(STATUS "SerdeTk headers not found (expected under third_party/SerdeTk or third_party/Klyspec/SerdeTk).")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/valijson/include")
    add_library(nuperf_valijson INTERFACE)
    target_include_directories(nuperf_valijson INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/valijson/include>
        $<INSTALL_INTERFACE:include>)
    install(TARGETS nuperf_valijson
        EXPORT nuperfTargets)
endif()
