cmake_minimum_required(VERSION 3.20)
project(AnyChatSDK VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(RUNTIME_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${RUNTIME_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${RUNTIME_OUTPUT_DIR})

# Use ccache when available to speed up builds.
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  message(STATUS "using ccache")
else()
  message(STATUS "ccache not found")
endif()

# ── Submodule guard ────────────────────────────────────────────────────────────
# Verify that submodules have been initialised.  A missing CMakeLists.txt in
# any thirdparty directory is a reliable indicator that the developer forgot to
# run `git submodule update --init --recursive`.
foreach(_dep curl googletest libwebsockets glaze)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/${_dep}/CMakeLists.txt")
    message(FATAL_ERROR
      "[AnyChat] Submodule 'thirdparty/${_dep}' is not initialised.\n"
      "  Run: git submodule update --init --recursive"
    )
  endif()
endforeach()

# Verify sqlite3 (not a submodule, directly bundled)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/sqlite3/CMakeLists.txt")
  message(FATAL_ERROR
    "[AnyChat] thirdparty/sqlite3 is missing.\n"
    "  Ensure sqlite3.c/sqlite3.h are present in thirdparty/sqlite3/"
  )
endif()

# ── Third-party dependencies ──────────────────────────────────────────────────

# ── glaze (header-only, requires C++23) ───────────────────────────────────────
add_subdirectory(thirdparty/glaze EXCLUDE_FROM_ALL)

# ── libcurl ───────────────────────────────────────────────────────────────────
# Build only the static library; disable the CLI tool and tests.
set(BUILD_CURL_EXE         OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING          OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_TESTS     ON  CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL       ON  CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL        OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY              ON  CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/curl EXCLUDE_FROM_ALL)
# curl's CMakeLists creates CURL::libcurl alias when BUILD_SHARED_LIBS=OFF.

# ── Copy OpenSSL DLLs to runtime directory (Windows only) ────────────────
if(WIN32 AND CURL_USE_OPENSSL)
    # Find OpenSSL (curl already did this, we just need the DLL paths)
    find_package(OpenSSL REQUIRED)

    # Get DLL directory from include directory
    get_filename_component(OPENSSL_ROOT_DIR "${OPENSSL_INCLUDE_DIR}" DIRECTORY)
    set(OPENSSL_BIN_DIR "${OPENSSL_ROOT_DIR}/bin")

    if(EXISTS "${OPENSSL_BIN_DIR}")
        # Copy OpenSSL DLLs to runtime directory
        file(GLOB OPENSSL_DLLS "${OPENSSL_BIN_DIR}/libcrypto*.dll" "${OPENSSL_BIN_DIR}/libssl*.dll")
        if(OPENSSL_DLLS)
            file(COPY ${OPENSSL_DLLS} DESTINATION ${RUNTIME_OUTPUT_DIR})
            message(STATUS "Copied OpenSSL DLLs to ${RUNTIME_OUTPUT_DIR}")
        endif()
    endif()
endif()

# ── libwebsockets ─────────────────────────────────────────────────────────────
set(LWS_WITHOUT_TESTAPPS          ON  CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_SERVER       ON  CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_PING         ON  CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_CLIENT       ON  CACHE BOOL "" FORCE)
set(LWS_WITHOUT_DAEMONIZE         ON  CACHE BOOL "" FORCE)
set(LWS_WITH_MINIMAL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(LWS_WITH_SSL                  ON  CACHE BOOL "" FORCE)
set(LWS_WITHOUT_EXTENSIONS        ON  CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/libwebsockets EXCLUDE_FROM_ALL)
# libwebsockets creates target: websockets (static) or websockets_shared.

# ── SQLite3 ───────────────────────────────────────────────────────────────────
# Direct sqlite3 amalgamation build (sqlite3.c / sqlite3.h).
# Creates targets: sqlite3 (STATIC) and SQLite::SQLite3 (ALIAS).
add_subdirectory(thirdparty/sqlite3 EXCLUDE_FROM_ALL)

# ── Build options ─────────────────────────────────────────────────────────────
option(BUILD_TESTS            "Build unit tests"                    ON)
option(BUILD_ANDROID_BINDING  "Build Android JNI binding"           OFF)
option(BUILD_IOS_BINDING      "Build iOS binding"                   OFF)
option(BUILD_WEB_BINDING      "Build WebAssembly binding"           OFF)

# ── googletest (only when tests are enabled) ──────────────────────────────────
if(BUILD_TESTS)
  set(INSTALL_GTEST             OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt    ON  CACHE BOOL "" FORCE)
  add_subdirectory(thirdparty/googletest EXCLUDE_FROM_ALL)
endif()

# ── Sub-directories ───────────────────────────────────────────────────────────
add_subdirectory(core)

if(BUILD_ANDROID_BINDING)
  add_subdirectory(packages/android)
endif()

if(BUILD_IOS_BINDING)
  add_subdirectory(packages/ios)
endif()

if(BUILD_WEB_BINDING)
  add_subdirectory(packages/web)
endif()

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(core/tests)
endif()
