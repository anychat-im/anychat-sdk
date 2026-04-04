add_library(anychat_core STATIC
    src/client.cpp
    src/auth_manager.cpp
    src/connection_manager.cpp
    src/notification_manager.cpp
    src/outbound_queue.cpp
    src/sync_engine.cpp
    src/message_manager.cpp
    src/conversation_manager.cpp
    src/friend_manager.cpp
    src/group_manager.cpp
    src/file_manager.cpp
    src/user_manager.cpp
    src/call_manager.cpp
    src/db/database.cpp
    src/db/migrations.cpp
    src/network/http_client.cpp
    src/network/websocket_client.cpp
)

target_include_directories(anychat_core
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(anychat_core PUBLIC cxx_std_17)

# Enable position-independent code for static libraries (needed for shared library linking)
set_target_properties(anychat_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ── Dependencies ──────────────────────────────────────────────────────────────

target_link_libraries(anychat_core
    PUBLIC  nlohmann_json::nlohmann_json
    PRIVATE CURL::libcurl
            SQLite::SQLite3
)

# libwebsockets: handle both target names produced by the submodule build.
if(TARGET websockets)
    target_link_libraries(anychat_core PRIVATE websockets)
elseif(TARGET websockets_shared)
    target_link_libraries(anychat_core PRIVATE websockets_shared)
else()
    message(FATAL_ERROR "[AnyChat] libwebsockets target not found. "
        "Ensure the thirdparty/libwebsockets submodule is initialised.")
endif()

# ── C API wrapper layer ───────────────────────────────────────────────────────

add_library(anychat_c STATIC
    src/c_api/errors_c.cpp
    src/c_api/utils_c.cpp
    src/c_api/client_c.cpp
    src/c_api/auth_c.cpp
    src/c_api/message_c.cpp
    src/c_api/conversation_c.cpp
    src/c_api/friend_c.cpp
    src/c_api/group_c.cpp
    src/c_api/file_c.cpp
    src/c_api/user_c.cpp
    src/c_api/call_c.cpp
)

target_include_directories(anychat_c
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/c_api
)

target_compile_features(anychat_c PUBLIC cxx_std_17)

# Enable position-independent code for static libraries (needed for shared library linking)
set_target_properties(anychat_c PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(anychat_c
    PUBLIC anychat_core
)

# Optional shared-library variant (for cross-compiler binary distribution).
option(BUILD_ANYCHAT_C_SHARED "Build C API as a shared library" OFF)
if(BUILD_ANYCHAT_C_SHARED)
    add_library(anychat_c_shared SHARED
        src/c_api/errors_c.cpp
        src/c_api/utils_c.cpp
        src/c_api/client_c.cpp
        src/c_api/auth_c.cpp
        src/c_api/message_c.cpp
        src/c_api/conversation_c.cpp
        src/c_api/friend_c.cpp
        src/c_api/group_c.cpp
        src/c_api/file_c.cpp
        src/c_api/user_c.cpp
    src/c_api/call_c.cpp
    )
    target_compile_definitions(anychat_c_shared
        PRIVATE ANYCHAT_C_EXPORTS
        PUBLIC ANYCHAT_C_SHARED
    )
    target_include_directories(anychat_c_shared
        PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/c_api
    )
    target_compile_features(anychat_c_shared PUBLIC cxx_std_17)
    target_link_libraries(anychat_c_shared PUBLIC anychat_core)
    set_target_properties(anychat_c_shared PROPERTIES
        OUTPUT_NAME anychat_c
        VERSION     ${PROJECT_VERSION}
        SOVERSION   0
    )
endif()
