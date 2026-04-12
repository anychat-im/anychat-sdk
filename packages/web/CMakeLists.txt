cmake_minimum_required(VERSION 3.20)
project(AnyChatWeb)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This CMakeLists.txt is for Emscripten only. Use emcmake cmake ...")
endif()

# Emscripten flags for WebAssembly
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the embind wrapper
add_executable(anychat_wasm
    src/embind_wrapper.cpp
)

target_link_libraries(anychat_wasm
    PRIVATE anychat_c
)

# Emscripten-specific linker flags
set_target_properties(anychat_wasm PROPERTIES
    SUFFIX ".js"
    OUTPUT_NAME "anychat"
)

# Emscripten linker options
# - MODULARIZE: Export as ES6 module
# - EXPORT_NAME: Name of the module export
# - EXPORTED_RUNTIME_METHODS: Runtime functions to expose
# - ALLOW_MEMORY_GROWTH: Allow heap to grow dynamically
# - ENVIRONMENT: Target web environment
# - POST_JS: Post-initialization JavaScript
target_link_options(anychat_wasm PRIVATE
    "SHELL:--bind"
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_NAME='createAnyChatModule'"
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['UTF8ToString','stringToUTF8']"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s ENVIRONMENT=web,worker"
    "SHELL:-s FILESYSTEM=0"
    "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
    "SHELL:--post-js ${CMAKE_CURRENT_SOURCE_DIR}/src/anychat_post.js"
)

# Copy TypeScript source files to build directory for npm packaging
file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ts")
foreach(ts_file ${TS_FILES})
    configure_file(${ts_file} ${CMAKE_CURRENT_BINARY_DIR}/src/ COPYONLY)
endforeach()

# Copy package files
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/package.json
    ${CMAKE_CURRENT_BINARY_DIR}/package.json
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/tsconfig.json
    ${CMAKE_CURRENT_BINARY_DIR}/tsconfig.json
    COPYONLY
)

# Installation rules
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/anychat.js
    ${CMAKE_CURRENT_BINARY_DIR}/anychat.wasm
    DESTINATION lib
)
