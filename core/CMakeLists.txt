include(GNUInstallDirs)

set(ANYCHAT_INTERNAL_SOURCES
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
    src/version_manager.cpp
    src/db/database.cpp
    src/db/migrations.cpp
    src/network/http_client.cpp
    src/network/websocket_client.cpp
)

set(ANYCHAT_C_API_SOURCES
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
    src/c_api/version_c.cpp
)

option(BUILD_ANYCHAT_SHARED "Build the public AnyChat C SDK as a shared library" ON)
set(_anychat_library_type STATIC)
if(BUILD_ANYCHAT_SHARED)
    set(_anychat_library_type SHARED)
endif()

add_library(anychat ${_anychat_library_type}
    ${ANYCHAT_INTERNAL_SOURCES}
    ${ANYCHAT_C_API_SOURCES}
)
add_library(AnyChat::anychat ALIAS anychat)

target_include_directories(anychat
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(anychat PRIVATE cxx_std_23)

set_target_properties(anychat PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

if(BUILD_ANYCHAT_SHARED)
    target_compile_definitions(anychat
        PRIVATE ANYCHAT_C_EXPORTS
        PUBLIC ANYCHAT_C_SHARED
    )
endif()

target_link_libraries(anychat
    PRIVATE glaze::glaze
    PRIVATE CURL::libcurl
            SQLite::SQLite3
)

if(TARGET websockets)
    target_link_libraries(anychat PRIVATE websockets)
elseif(TARGET websockets_shared)
    target_link_libraries(anychat PRIVATE websockets_shared)
else()
    message(FATAL_ERROR "[AnyChat] libwebsockets target not found. "
        "Ensure the thirdparty/libwebsockets submodule is initialised.")
endif()

install(TARGETS anychat
    EXPORT AnyChatTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/anychat"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
