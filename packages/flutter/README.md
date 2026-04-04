# anychat_sdk

Official Flutter SDK for the AnyChat instant messaging system. Build cross-platform IM applications for Android, iOS, Windows, Linux, and macOS with native FFI bindings for high performance.

[![pub package](https://img.shields.io/pub/v/anychat_sdk.svg)](https://pub.dev/packages/anychat_sdk)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)

## Features

- **Authentication**: Login, register, token management, auto token refresh
- **Messaging**: Send text/media messages, fetch history, read receipts, typing indicators
- **Conversations**: List, pin, mute, delete conversations with real-time updates
- **Friends**: Friend requests, blacklist management, friend search
- **Groups**: Create, join, invite, manage members with role-based permissions
- **File Transfer**: Upload/download files with progress tracking
- **Call**: Voice/video calls, multi-party meetings with WebRTC
- **User**: Profile management, settings, user search
- **Real-time**: WebSocket-based real-time messaging with auto-reconnect
- **Offline Storage**: SQLite-based local database for offline access

## Installation

Add this to your package's `pubspec.yaml` file:

```yaml
dependencies:
  anychat_sdk: ^0.1.0
  path_provider: ^2.0.0  # For database path
```

Then run:

```bash
flutter pub get
```

## Platform Support

| Platform | Support | Min Version |
|----------|---------|-------------|
| Android  | ✅      | API 21+     |
| iOS      | ✅      | iOS 12+     |
| macOS    | ✅      | macOS 10.14+|
| Linux    | ✅      | -           |
| Windows  | ✅      | Windows 10+ |
| Web      | ❌      | -           |

## Quick Start

```dart
import 'package:anychat_sdk/anychat_sdk.dart';
import 'package:path_provider/path_provider.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // 1. Get database path
  final appDir = await getApplicationSupportDirectory();
  final dbPath = '${appDir.path}/anychat.db';

  // 2. Create client
  final client = AnyChatClient(
    gatewayUrl: 'wss://api.anychat.io',
    apiBaseUrl: 'https://api.anychat.io/api/v1',
    deviceId: 'unique-device-id',  // Generate and persist a UUID
    dbPath: dbPath,
    connectTimeoutMs: 10000,
    maxReconnectAttempts: 5,
    autoReconnect: true,
  );

  // 3. Listen to connection state
  client.connectionStateStream.listen((state) {
    print('Connection state: $state');
  });

  // 4. Connect
  client.connect();

  // 5. Login
  try {
    final token = await client.login(
      account: 'user@example.com',
      password: 'password',
    );
    print('Logged in successfully');
  } catch (e) {
    print('Login failed: $e');
  }

  // 6. Listen for incoming messages
  client.messageReceivedStream.listen((message) {
    print('New message: ${message.content}');
  });

  // 7. Send a message
  await client.sendTextMessage(
    sessionId: 'conv-123',
    content: 'Hello from Flutter!',
  );

  // 8. Get conversations
  final conversations = await client.getConversations();
  for (final conv in conversations) {
    print('${conv.convId}: ${conv.lastMsgText} (${conv.unreadCount} unread)');
  }

  // 9. Clean up when done
  client.dispose();
}
```

## Usage Examples

### Authentication

```dart
// Login with account and password
final token = await client.login(
  account: 'user@example.com',
  password: 'password',
);

// Check if logged in
if (client.isLoggedIn) {
  print('User is logged in');
}

// Get current token
final currentToken = client.currentToken;
print('Token expires at: ${currentToken?.expiresAtMs}');

// Logout
await client.logout();
```

### Messaging

```dart
// Send text message
await client.sendTextMessage(
  sessionId: 'conv-123',
  content: 'Hello!',
);

// Get message history
final messages = await client.getMessageHistory(
  sessionId: 'conv-123',
  beforeTimestampMs: DateTime.now().millisecondsSinceEpoch,
  limit: 20,
);

// Listen for new messages
client.messageReceivedStream.listen((message) {
  print('From: ${message.senderId}, Content: ${message.content}');
});
```

### Conversations

```dart
// Get all conversations
final conversations = await client.getConversations();

// Mark conversation as read
await client.markConversationRead('conv-123');

// Listen for conversation updates
client.conversationUpdatedStream.listen((conversation) {
  print('Conversation updated: ${conversation.convId}');
});
```

### Friends

```dart
// Get friend list
final friends = await client.getFriends();

// Listen for friend requests
client.friendRequestStream.listen((request) {
  print('Friend request from: ${request.fromUserId}');
});
```

### RTC Calls

```dart
// Listen for incoming calls
client.incomingCallStream.listen((call) {
  print('Incoming call from: ${call.callerId}');
});
```

## Architecture

This package uses Dart FFI to call native C libraries, providing:

- **Native Performance**: No message serialization overhead
- **Cross-Platform**: Same codebase for all platforms
- **Type Safety**: Strongly-typed Dart API

```
┌────────────────────────────┐
│  Your Flutter App          │
└────────────────────────────┘
            ↓
┌────────────────────────────┐
│  anychat_sdk (Dart)        │
│  - High-level API          │
│  - Stream-based events     │
│  - Future-based async ops  │
└────────────────────────────┘
            ↓ dart:ffi
┌────────────────────────────┐
│  anychat_c (C ABI)         │
│  - Stable C interface      │
│  - Opaque handles          │
└────────────────────────────┘
            ↓ C++
┌────────────────────────────┐
│  anychat_core (C++)        │
│  - WebSocket, HTTP, SQLite │
│  - Business logic          │
└────────────────────────────┘
```

## Configuration

### Android

No additional configuration required. The native library is built automatically via CMake.

### iOS

No additional configuration required. The native library is built automatically via CocoaPods.

### macOS

Enable network and file access in `macos/Runner/DebugProfile.entitlements` and `Release.entitlements`:

```xml
<key>com.apple.security.network.client</key>
<true/>
<key>com.apple.security.files.user-selected.read-write</key>
<true/>
```

### Linux

Install required dependencies:

```bash
sudo apt-get install libcurl4-openssl-dev libssl-dev
```

### Windows

No additional configuration required. Dependencies are statically linked.

## API Documentation

Full API documentation is available at [pub.dev/documentation/anychat_sdk](https://pub.dev/documentation/anychat_sdk).

## Known Limitations

- **Async Callbacks**: Some async operations use placeholder implementations. Full implementation requires `NativeCallable` (Dart 2.18+).
- **Web Support**: Not supported. This package uses native FFI which is not available on Web.

## Troubleshooting

### Build Errors

If you encounter build errors, try:

```bash
flutter clean
flutter pub get
flutter run
```

### Connection Issues

Ensure your server URL is correct and accessible. Check the connection state:

```dart
client.connectionStateStream.listen((state) {
  print('Connection: $state');
});
```

## Contributing

Contributions are welcome! Please read the [contributing guide](https://github.com/yzhgit/anychat-sdk/blob/main/CONTRIBUTING.md) first.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Links

- [GitHub Repository](https://github.com/yzhgit/anychat-sdk)
- [Issue Tracker](https://github.com/yzhgit/anychat-sdk/issues)
- [Backend API Documentation](https://yzhgit.github.io/anychat-server)
- [Backend Repository](https://github.com/yzhgit/anychat-server)

## Support

For questions and support, please open an issue on [GitHub](https://github.com/yzhgit/anychat-sdk/issues).
