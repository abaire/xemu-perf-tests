include(ExternalProject)

# Full featured printf
set(PRINTF_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/third_party/printf-src")
set(PRINTF_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/third_party/printf-build")
set(PRINTF_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/third_party/printf-install")
set(PRINTF_ARTIFACT "${PRINTF_INSTALL_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}printf${CMAKE_STATIC_LIBRARY_SUFFIX}")
ExternalProject_Add(
        printf_project
        GIT_REPOSITORY "https://github.com/mpaland/printf.git"
        GIT_TAG "master"
        GIT_SHALLOW TRUE

        SOURCE_DIR "${PRINTF_SOURCE_DIR}"
        BINARY_DIR "${PRINTF_BINARY_DIR}"
        INSTALL_DIR "${PRINTF_INSTALL_DIR}"

        CONFIGURE_COMMAND ""

        BUILD_COMMAND
        "${CMAKE_C_COMPILER}"
        -O3
        -Wno-everything
        -c "${PRINTF_SOURCE_DIR}/printf.c"
        -o "${PRINTF_BINARY_DIR}/printf.o"
        COMMAND
        "${CMAKE_AR}"
        rc
        "${PRINTF_INSTALL_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}printf${CMAKE_STATIC_LIBRARY_SUFFIX}"
        "${PRINTF_BINARY_DIR}/printf.o"

        INSTALL_COMMAND ""

        BUILD_BYPRODUCTS
        "${PRINTF_ARTIFACT}"

        LOG_BUILD ON
)

add_library(printf STATIC IMPORTED GLOBAL)

target_include_directories(
        printf
        INTERFACE
        "${PRINTF_SOURCE_DIR}"
)

set_property(
        TARGET printf
        PROPERTY IMPORTED_LOCATION
        "${PRINTF_ARTIFACT}"
)

add_dependencies(
        printf
        printf_project
)

add_library(Printf::printf ALIAS printf)


# tiny-json
set(TINYJSON_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/third_party/tinyjson-src")
set(TINYJSON_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/third_party/tinyjson-build")
set(TINYJSON_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/third_party/tinyjson-install")
set(TINYJSON_ARTIFACT "${TINYJSON_INSTALL_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}tinyjson${CMAKE_STATIC_LIBRARY_SUFFIX}")
ExternalProject_Add(
        tinyjson_project
        GIT_REPOSITORY "https://github.com/rafagafe/tiny-json.git"
        GIT_TAG "master"
        GIT_SHALLOW TRUE

        SOURCE_DIR "${TINYJSON_SOURCE_DIR}"
        BINARY_DIR "${TINYJSON_BINARY_DIR}"
        INSTALL_DIR "${TINYJSON_INSTALL_DIR}"

        CONFIGURE_COMMAND ""

        BUILD_COMMAND
        "${CMAKE_C_COMPILER}"
        -O3
        -Wno-everything
        -c "${TINYJSON_SOURCE_DIR}/tiny-json.c"
        -o "${TINYJSON_BINARY_DIR}/tiny-json.o"
        COMMAND
        "${CMAKE_AR}"
        rc
        "${TINYJSON_INSTALL_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}tinyjson${CMAKE_STATIC_LIBRARY_SUFFIX}"
        "${TINYJSON_BINARY_DIR}/tiny-json.o"

        INSTALL_COMMAND ""

        BUILD_BYPRODUCTS
        "${TINYJSON_ARTIFACT}"

        LOG_BUILD ON
)

add_library(tinyjson STATIC IMPORTED GLOBAL)

target_include_directories(
        tinyjson
        INTERFACE
        "${TINYJSON_SOURCE_DIR}"
)

set_property(
        TARGET tinyjson
        PROPERTY IMPORTED_LOCATION
        "${TINYJSON_ARTIFACT}"
)

add_dependencies(
        tinyjson
        tinyjson_project
)

add_library(TinyJSON::tinyjson ALIAS tinyjson)
