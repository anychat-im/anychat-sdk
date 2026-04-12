# Linux platform build configuration for anychat_flutter
cmake_minimum_required(VERSION 3.10)

project(anychat_flutter_linux LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Resolve the real path of this CMakeLists.txt (following symlinks)
# This is necessary because Flutter's plugin system uses symlinks
get_filename_component(PLUGIN_DIR "${CMAKE_CURRENT_LIST_DIR}" REALPATH)
get_filename_component(ANYCHAT_SDK_ROOT "${PLUGIN_DIR}/../../.." ABSOLUTE)

# ── Third-party dependencies (required by core) ──────────────────────────────

# ── libcurl ───────────────────────────────────────────────────────────────────
set(BUILD_CURL_EXE         OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING          OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_TESTS     ON  CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL       ON  CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL        OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY              ON  CACHE BOOL "" FORCE)
add_subdirectory("${ANYCHAT_SDK_ROOT}/thirdparty/curl" curl_build EXCLUDE_FROM_ALL)

# ── libwebsockets ─────────────────────────────────────────────────────────────
set(LWS_WITHOUT_TESTAPPS          ON  CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_SERVER       ON  CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_PING         ON  CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_CLIENT       ON  CACHE BOOL "" FORCE)
set(LWS_WITHOUT_DAEMONIZE         ON  CACHE BOOL "" FORCE)
set(LWS_WITH_MINIMAL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(LWS_WITH_SSL                  ON  CACHE BOOL "" FORCE)
set(LWS_WITHOUT_EXTENSIONS        ON  CACHE BOOL "" FORCE)
add_subdirectory("${ANYCHAT_SDK_ROOT}/thirdparty/libwebsockets" libwebsockets_build EXCLUDE_FROM_ALL)

# ── SQLite3 ───────────────────────────────────────────────────────────────────
add_subdirectory("${ANYCHAT_SDK_ROOT}/thirdparty/sqlite3" sqlite3_build EXCLUDE_FROM_ALL)

# Include the core build
add_subdirectory("${ANYCHAT_SDK_ROOT}/core" anychat_core_build)

# Create the Flutter plugin shared library
# This is a thin wrapper that re-exports anychat_c for Flutter FFI
add_library(anychat_flutter_plugin SHARED
    plugin.cpp
)

target_link_libraries(anychat_flutter_plugin
    PUBLIC -Wl,--whole-archive anychat_c -Wl,--no-whole-archive
)

set_target_properties(anychat_flutter_plugin PROPERTIES
    PUBLIC_HEADER "${ANYCHAT_SDK_ROOT}/core/include/anychat_c/anychat_c.h"
    LINK_FLAGS "-Wl,--no-undefined"
)

install(TARGETS anychat_flutter_plugin
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/anychat_c
)
