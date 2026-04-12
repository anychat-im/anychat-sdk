cmake_minimum_required(VERSION 3.22.1)

project(anychat_android)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the core SDK
set(ANYCHAT_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../core")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp
    ${ANYCHAT_CORE_DIR}/include
    ${ANYCHAT_CORE_DIR}/include/anychat_c
)

# Add the core library (assumes it's already built or will be built)
add_subdirectory(${ANYCHAT_CORE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/core)

# JNI bridge sources
set(JNI_SOURCES
    src/main/cpp/jni_bridge.cpp
    src/main/cpp/jni_helpers.cpp
    src/main/cpp/jni_auth.cpp
    src/main/cpp/jni_message.cpp
    src/main/cpp/jni_conversation.cpp
    src/main/cpp/jni_friend.cpp
    src/main/cpp/jni_group.cpp
)

# Create the shared library
add_library(anychat_jni SHARED ${JNI_SOURCES})

# Link against the core library and Android log
target_link_libraries(anychat_jni
    anychat_core
    android
    log
)

# Set output directory
set_target_properties(anychat_jni PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}"
)
