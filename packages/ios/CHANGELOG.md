# Changelog

All notable changes to AnyChatSDK will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-02-19

### Added
- Initial CocoaPods release
- Complete Swift API with async/await support
- AsyncStream-based event handling
- Full type safety with Swift structs and enums
- Thread-safe actor-based design
- Authentication module (login, register, logout, token refresh)
- Messaging module (send, receive, history, mark read)
- Conversation module (list, mark read, pin, mute, delete)
- Friend module (list, add, remove, blacklist, requests)
- Group module (list, create, join, invite, members, quit)
- User module (profile, settings, search, push tokens)
- File module (upload, download, delete with progress)
- Call module (voice/video calls, meetings, call logs)
- Automatic C resource memory management
- Connection state monitoring via AsyncStream
- iOS 13.0+ and macOS 10.15+ support
- Swift 5.9+ compatibility
- Example iOS app demonstrating SDK usage
- Comprehensive documentation and README

### Features
- Modern Swift concurrency with actors
- Automatic reconnection with exponential backoff
- WebSocket heart beat (30s interval)
- SQLite-based local storage
- Type-safe error handling with AnyChatError enum
- Progress callbacks for file uploads
- Call status monitoring with AsyncStream
- Token expiration events
- Friend list change notifications
- Group invitation events
- Message received events

### Supported Platforms
- iOS 13.0+
- macOS 10.15+
- tvOS 13.0+ (not yet tested)
- watchOS 6.0+ (not yet tested)

## [Unreleased]

### Planned
- Push notification support
- Offline message sync
- Message reactions and replies
- Typing indicators
- Read receipts
- Media message previews
- Voice message recording
- Screen sharing support
- Meeting recording
- End-to-end encryption
- Multi-device sync
- Background message fetching
- Widget support
- App Clips support
- SwiftUI view components
- UIKit view components

---

[0.1.0]: https://github.com/yzhgit/anychat-sdk/releases/tag/v0.1.0
