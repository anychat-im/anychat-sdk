add_executable(anychat_core_tests
    test_types.cpp
    test_client.cpp
    test_connection_manager.cpp
    test_database.cpp
    test_cache.cpp
    test_auth.cpp
    test_notification_manager.cpp
    test_outbound_queue.cpp
    test_sync_engine.cpp
    test_message_manager.cpp
    test_conversation_manager.cpp
    test_friend_manager.cpp
    test_group_manager.cpp
    test_file_manager.cpp
    test_user_manager.cpp
    test_call_manager.cpp
)

target_link_libraries(anychat_core_tests
    PRIVATE anychat_core
    PRIVATE GTest::gtest_main
)

# 测试代码需要访问 core 的内部头文件（src/ 目录）
target_include_directories(anychat_core_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

include(GoogleTest)
gtest_discover_tests(anychat_core_tests)
