cmake_minimum_required(VERSION 3.16)
project(surge-ext VERSION 2.0 LANGUAGES C)

# Keep host-specific instructions optional so container images are portable.
option(NATIVE_OPTIMIZATIONS "Optimize Release builds for the build host CPU" ON)
if (NATIVE_OPTIMIZATIONS)
    add_compile_options(
        $<$<CONFIG:Release>:-march=native>
        $<$<CONFIG:Release>:-mtune=native>
    )
endif()

# NAUTY package
set(NAUTY_DIR "${CMAKE_SOURCE_DIR}/nauty" CACHE PATH "Path to nauty headers")
set(NAUTY_LIB "${NAUTY_DIR}/nautyW1.a" CACHE FILEPATH "Path to the nauty library")
include_directories(${NAUTY_DIR})

# ZLIB option
option(USE_ZLIB "Link with zlib" OFF)
if (USE_ZLIB)
    add_definitions(-DZLIB)
    find_library(ZLIB_LIB z)
endif()

# General definitions
add_definitions(
        -DWORDSIZE=32
        -DMAXN=WORDSIZE
        -DOUTPROC=surgeproc
        -DPREPRUNE=surgepreprune
        -DPRUNE=surgeprune
        -DGENG_MAIN=geng_main
)

set(COMMON_SOURCES
    src/surge.c src/included/geng.c src/included/planarity.c src/utils.c
    src/config.c src/element.c src/output.c src/variable_valence.c
    src/pipeline.c src/formula.c src/geng_driver.c src/prune.c
    src/vcolg.c src/multig.c src/cycles.c src/smiles_skel.c
    src/planarity_util.c src/vv_sched.c src/vv_progress.c src/vv_merge.c
)

# Executables
add_executable(surge-ext ${COMMON_SOURCES})
target_link_libraries(surge-ext PRIVATE ${NAUTY_LIB} ${ZLIB_LIB})

# Plugins
set(PLUGIN_DIR "${CMAKE_SOURCE_DIR}/src/plugins")

file(GLOB PLUGIN_SOURCES CONFIGURE_DEPENDS "${PLUGIN_DIR}/plugin*.c")

if(PLUGIN_SOURCES STREQUAL "")
    message(STATUS "No plugins found in: ${PLUGIN_DIR}")
else()
    set(_all_plugin_targets "")
    foreach(PLUGIN_SRC IN LISTS PLUGIN_SOURCES)
        get_filename_component(PLUGIN_BASENAME "${PLUGIN_SRC}" NAME_WE)   # e.g. "plugin0"

        # If the name is plugin<number> -> use the number (compatible with surge_0..)
        if(PLUGIN_BASENAME MATCHES "^plugin([0-9]+)$")
            set(TARGET_NAME "surge-ext_${CMAKE_MATCH_1}")
        else()
            # otherwise sanitize the basename
            string(REGEX REPLACE "[^A-Za-z0-9_]+" "_" SAFE "${PLUGIN_BASENAME}")
            set(TARGET_NAME "surge-ext_${SAFE}")
        endif()

        message(STATUS "Plugin-Target: ${TARGET_NAME}  (Source: ${PLUGIN_SRC})")

        add_executable(${TARGET_NAME} ${COMMON_SOURCES})

        target_link_libraries(${TARGET_NAME} PRIVATE ${NAUTY_LIB} ${ZLIB_LIB})

        set(_plugin_literal "\"${PLUGIN_SRC}\"")
        target_compile_definitions(${TARGET_NAME} PRIVATE SURGEPLUGIN=${_plugin_literal})

        list(APPEND _all_plugin_targets ${TARGET_NAME})
    endforeach()

endif()

# Debug build
add_executable(surge-ext_debug EXCLUDE_FROM_ALL ${COMMON_SOURCES})
set_target_properties(surge-ext_debug PROPERTIES
        COMPILE_FLAGS "-g -O0"
)
target_link_libraries(surge-ext_debug PRIVATE ${NAUTY_LIB} ${ZLIB_LIB})
