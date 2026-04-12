# Changelog

All notable changes to the AnyChat Android SDK will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned
- Message encryption (E2EE)
- Voice/video call support
- Push notification integration
- File upload/download progress callbacks
- Message read receipts
- Typing indicators

## [0.1.0] - 2026-02-19

### Added
- Initial release of AnyChat Android SDK
- Core client initialization with `AnyChatClient`
- Authentication module (`Auth`)
  - User registration
  - Login/logout
  - Token refresh
- Message module (`Message`)
  - Send/receive text messages
  - Message history pagination
  - Real-time message flow
- Conversation module (`Conversation`)
  - List conversations
  - Get conversation details
  - Mark as read
- Friend module (`Friend`)
  - Search users
  - Send/accept/reject friend requests
  - Get friend list
  - Delete friends
- Group module (`Group`)
  - Create groups
  - Add/remove members
  - Update group info
  - List group members
- WebSocket connection with auto-reconnect
- Local SQLite database caching
- Coroutine-based async API
- Multi-ABI support (arm64-v8a, armeabi-v7a, x86, x86_64)
- ProGuard rules for release builds

### Dependencies
- Kotlin 1.9.0
- Kotlin Coroutines 1.7.3
- AndroidX Core KTX 1.12.0
- Minimum SDK: API 24 (Android 7.0)
- Target SDK: API 34 (Android 14)

### Native Libraries
- libcurl (HTTP client)
- libwebsockets (WebSocket client)
- SQLite3 (local database)
- glaze (JSON parsing)

[Unreleased]: https://github.com/yzhgit/anychat-sdk/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/yzhgit/anychat-sdk/releases/tag/v0.1.0
