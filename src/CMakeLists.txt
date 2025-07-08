set(XBE_TITLE "Perf tests")

include(XBEUtils REQUIRED)

find_package(NXDK REQUIRED)

find_package(NXDK_SDL2 REQUIRED)
find_package(NXDK_SDL2_Image REQUIRED)
find_package(Threads REQUIRED)

include(FetchContent)

FetchContent_Declare(
        pbkitplusplus
        GIT_REPOSITORY https://github.com/abaire/pbkitplusplus
        GIT_TAG main
)

set(NO_OPT ON CACHE INTERNAL "Force pbkitplusplus debug build")

FetchContent_MakeAvailable(pbkitplusplus)

configure_file(configure.h.in configure.h)

macro(set_opt_compile_and_link_options TARGET_NAME)
    target_compile_options(
            "${TARGET_NAME}"
            PRIVATE
            -O3
            -fno-strict-aliasing
            -Wall
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-builtin-macro-redefined>   # Suppress warning from NXDK undef of __STDC_NO_THREADS__
            -D_USE_MATH_DEFINES
    )
    target_link_options(
            "${TARGET_NAME}"
            PRIVATE
            "/debug:none"
    )
endmacro()

macro(set_compile_and_link_options TARGET_NAME)
    if (CMAKE_BUILD_TYPE MATCHES Debug)
        target_compile_options(
                "${TARGET_NAME}"
                PRIVATE
                -g
                -gdwarf-4
                -O0
                -Wall
                -fstandalone-debug
                $<$<COMPILE_LANGUAGE:CXX>:-Wno-builtin-macro-redefined>   # Suppress warning from NXDK undef of __STDC_NO_THREADS__
                -D_USE_MATH_DEFINES
        )
        target_link_options(
                "${TARGET_NAME}"
                PRIVATE
                "/debug:full"
        )
    else ()
        set_opt_compile_and_link_options("${TARGET_NAME}")
    endif ()
endmacro()

# Vertex shaders compiled using https://pypi.org/project/nv2a-vsh/
include(NV2A_VSH REQUIRED)
generate_nv2a_vshinc_files(
        OUTPUT_VARIABLE _VERTEX_SHADER_FILES
        INCLUDE_DIRECTORIES_VARIABLE _VERTEX_SHADER_INCLUDE_DIRS
        GENERATION_TARGET_VARIABLE _VERTEX_SHADER_GEN_TARGET
        SOURCES
        shaders/passthrough.vsh
)

add_library(
        perf_tests
        tests/high_vertex_count_tests.cpp
        tests/high_vertex_count_tests.h
        tests/vertex_buffer_allocation_tests.cpp
        tests/vertex_buffer_allocation_tests.h
        tests/test_suite.cpp
        tests/test_suite.h
)

set_compile_and_link_options(perf_tests)

target_include_directories(
        perf_tests
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        ${_VERTEX_SHADER_INCLUDE_DIRS}
        ${PRINTF_SOURCE_DIR}
)


target_link_libraries(
        perf_tests
        PRIVATE
        Printf::printf
        pbkitplusplus
        NXDK::NXDK
        NXDK::NXDK_CXX
        NXDK::SDL2
        NXDK::SDL2_Image
)

add_dependencies(
        perf_tests
        ${_VERTEX_SHADER_GEN_TARGET}
)

add_executable(
        ${PROJECT_NAME}
        main.cpp
        debug_output.cpp
        debug_output.h
        logger.cpp
        logger.h
        menu_item.cpp
        menu_item.h
        runtime_config.cpp
        runtime_config.h
        test_driver.cpp
        test_driver.h
        test_host.cpp
        test_host.h
)

# Pull debug info out of the binary into a host-side linked binary.
split_debug(${PROJECT_NAME})

set(EXECUTABLE_BINARY "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.exe")
set_compile_and_link_options(${PROJECT_NAME})
target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE
        perf_tests
        Printf::printf
        TinyJSON::tinyjson
        pbkitplusplus
        NXDK::NXDK
        NXDK::NXDK_CXX
        NXDK::SDL2
        NXDK::SDL2_Image
)

add_xbe(
        xbe_file "${EXECUTABLE_BINARY}"
        TITLE "${XBE_TITLE}"
        RESOURCE_ROOTS
        "${CMAKE_SOURCE_DIR}/resources"
        RESOURCE_DIRS
        "${CMAKE_SOURCE_DIR}/resources"
)
add_xiso(${PROJECT_NAME}_xiso xbe_file)
