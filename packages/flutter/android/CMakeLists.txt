# Android platform build configuration for anychat_flutter
cmake_minimum_required(VERSION 3.10)

project(anychat_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Add the core C API library ────────────────────────────────────────────────
# Assumes the SDK root is located at ../../.. from this directory
set(ANYCHAT_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

# Include the core build (anychat_core + anychat_c)
add_subdirectory("${ANYCHAT_SDK_ROOT}/core" anychat_core_build)

# ── Create the Android shared library ─────────────────────────────────────────
# This wraps anychat_c and is loaded by Dart FFI
add_library(anychat_android SHARED
    # No additional sources needed; we're just re-exporting anychat_c
)

target_link_libraries(anychat_android
    PUBLIC anychat_c
)

# Export all symbols from anychat_c
set_target_properties(anychat_android PROPERTIES
    LINK_FLAGS "-Wl,--whole-archive $<TARGET_FILE:anychat_c> -Wl,--no-whole-archive"
)

# Strip debug symbols for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(anychat_android PROPERTIES
        LINK_FLAGS_RELEASE "-s"
    )
endif()
