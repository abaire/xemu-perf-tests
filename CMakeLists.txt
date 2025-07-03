cmake_minimum_required(VERSION 3.30)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
include(PrebuildNXDK)

project(xemu-perf-tests)

set(
        CMAKE_MODULE_PATH
        ${CMAKE_MODULE_PATH}
        "${CMAKE_CURRENT_LIST_DIR}/cmake/modules/"
)

set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "Generate verbose Makefile")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(_CMAKE_PROCESSING_LANGUAGE "CXX")

# Determine if this build is for the Xbox or the host system.
if (CMAKE_TOOLCHAIN_FILE MATCHES "toolchain-nxdk.cmake")
    set(IS_TARGET_BUILD ON)
else ()
    message (FATAL_ERROR "This project must be built with the nxdk toolchain (`-DCMAKE_TOOLCHAIN_FILE=<YOUR_NXDK_DIR>/share/toolchain-nxdk.cmake`)")
endif ()

option(
        NO_OPT
        "Suppress optimization on core files that are typically optimized regardless of release/debug configuration"
        OFF
)

option(
        DISABLE_AUTORUN
        "Prevents the tests from being executed without user input."
        OFF
)

option(
        AUTORUN_IMMEDIATELY
        "Remove the delay for input before starting automated testing."
        OFF
)

option(
        ENABLE_SHUTDOWN
        "Cause the program to shut down the xbox on completion instead of rebooting."
        OFF
)

set(
        RUNTIME_CONFIG_PATH
        "e:/xemu_perf_tests/xemu_perf_tests_config.json"
        CACHE STRING
        "Absolute XBOX-path to the location of a JSON configuration file to set options and filter the tests that are executed."
)

set(
        DEFAULT_OUTPUT_DIRECTORY_PATH
        "e:/xemu_perf_tests"
        CACHE STRING
        "Absolute XBOX-path to a directory into which test output will be written."
)

option(
        SKIP_TESTS_BY_DEFAULT
        "Causes all tests whose names do not appear in the runtime config to be skipped when automatically executing tests."
        OFF
)

add_subdirectory(third_party)
add_subdirectory(src)
