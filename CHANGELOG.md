# Changelog

All notable changes to AnyChatSDK will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- C++ core SDK with full IM features (auth, messaging, conversations, friends, groups, files, Call)
- C API layer (`anychat_c`) for stable cross-compiler ABI
  - Opaque handle types for type safety
  - C callbacks with userdata for async operations
  - Thread-local error messages via `anychat_get_last_error()`
  - Memory management functions (`anychat_free_*`)
- Third-party dependencies as Git submodules (libcurl, libwebsockets, nlohmann-json, googletest)
- SQLite3 amalgamation for local database (no system dependency)
- Flutter FFI bindings (`anychat_flutter` package)
  - Auto-generated bindings via `ffigen`
  - High-level Dart API with Future-based async and Stream-based events
  - Cross-platform support (Android, iOS, Linux, macOS, Windows)
  - Example app demonstrating core features
- C API usage guide (`docs/c_api_guide.md`)
- Platform build configurations:
  - Android: CMakeLists.txt for NDK build
  - iOS/macOS: CocoaPods .podspec files
  - Linux/Windows: CMakeLists.txt for desktop builds

### Removed
- SWIG bindings (replaced by platform-native bindings directly using C API)
- System dependencies for third-party libraries (now use submodules)

### Changed
- Architecture: moved from SWIG → direct platform bindings (JNI, FFI, bridging headers)
- Database: replaced ormpp with raw SQLite C API for better control and thread safety
- Build system: unified CMake configuration for all platforms

## [0.1.0] - 2024-02-18 (Initial Release)

### Added
- Project scaffold
- Repository structure
- Git submodule configuration
