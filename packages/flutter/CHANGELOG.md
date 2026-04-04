# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-02-19

### Added

- Initial release of AnyChat SDK for Flutter
- Authentication module: Login, logout, token management
- Messaging module: Send text messages, fetch history
- Conversation module: List conversations, mark as read
- Friend module: Get friend list, handle friend requests
- Group module: Basic group operations (placeholder)
- File module: File transfer support (placeholder)
- Call module: Voice/video calls (placeholder)
- User module: Profile management (placeholder)
- WebSocket-based real-time communication with auto-reconnect
- SQLite-based local storage for offline access
- Cross-platform support: Android, iOS, Windows, Linux, macOS
- FFI-based native bindings for high performance
- Stream-based event handling for real-time updates
- Future-based async operations

### Known Issues

- Async callback implementations are placeholders requiring `NativeCallable`
- Some advanced features are not yet fully implemented
- Web platform is not supported (FFI limitation)

## [Unreleased]

### Planned

- Complete async callback implementations with `NativeCallable`
- Enhanced error handling and retry logic
- Message encryption/decryption support
- Media message support (images, files, audio, video)
- Push notification integration
- Message search functionality
- Advanced group management features
- Call screen sharing support
- Performance optimizations
- Comprehensive example app
- Unit and integration tests
- API documentation improvements

---

[0.1.0]: https://github.com/yzhgit/anychat-sdk/releases/tag/v0.1.0
