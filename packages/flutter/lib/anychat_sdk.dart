/// AnyChat IM SDK for Flutter
///
/// Official Flutter SDK for building cross-platform instant messaging applications
/// with the AnyChat IM system. Supports Android, iOS, Windows, Linux, and macOS.
///
/// ## Features
///
/// - Authentication: Login, register, token management
/// - Messaging: Send text/media, fetch history, read receipts
/// - Conversations: List, pin, mute, delete conversations
/// - Friends: Friend requests, blacklist management
/// - Groups: Create, join, invite, manage members
/// - File Transfer: Upload/download with progress tracking
/// - Call: Voice/video calls, meetings
/// - User: Profile, settings, search
///
/// ## Quick Start
///
/// ```dart
/// import 'package:anychat_sdk/anychat_sdk.dart';
/// import 'package:path_provider/path_provider.dart';
///
/// // Create client
/// final dbDir = await getApplicationSupportDirectory();
/// final client = AnyChatClient(
///   gatewayUrl: 'wss://api.anychat.io',
///   apiBaseUrl: 'https://api.anychat.io/api/v1',
///   deviceId: '<unique-device-id>',
///   dbPath: '${dbDir.path}/anychat.db',
/// );
///
/// // Listen to connection state
/// client.connectionStateStream.listen((state) {
///   print('Connection state: $state');
/// });
///
/// // Connect
/// client.connect();
///
/// // Login
/// final token = await client.login(
///   account: 'user@example.com',
///   password: 'password',
/// );
///
/// // Send a message
/// await client.sendTextMessage(
///   sessionId: 'conv-123',
///   content: 'Hello!',
/// );
///
/// // Listen for incoming messages
/// client.messageReceivedStream.listen((message) {
///   print('Received: ${message.content}');
/// });
///
/// // Clean up
/// client.dispose();
/// ```
///
/// ## Documentation
///
/// - [GitHub Repository](https://github.com/yzhgit/anychat-sdk)
/// - [Backend API Documentation](https://yzhgit.github.io/anychat-server)
/// - [Backend Repository](https://github.com/yzhgit/anychat-server)
///
library anychat_sdk;

// Core SDK exports
export 'src/anychat_client.dart';
export 'src/models.dart';
