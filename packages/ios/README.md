# AnyChatSDK for iOS/macOS

[![CocoaPods Compatible](https://img.shields.io/cocoapods/v/AnyChatSDK.svg)](https://cocoapods.org/pods/AnyChatSDK)
[![Platform](https://img.shields.io/cocoapods/p/AnyChatSDK.svg?style=flat)](https://cocoapods.org/pods/AnyChatSDK)
[![Swift](https://img.shields.io/badge/Swift-5.9-orange.svg)](https://swift.org)
[![License](https://img.shields.io/cocoapods/l/AnyChatSDK.svg?style=flat)](https://github.com/yzhgit/anychat-sdk/blob/main/LICENSE)

Complete Swift SDK for the AnyChat instant messaging system, providing messaging, voice/video calls, group management, and real-time communication features.

## Features

- **Modern Swift API** with async/await
- **AsyncStream-based** event handling
- **Full type safety** with Swift structs and enums
- **Thread-safe** actor-based design
- **Automatic memory management**
- **Voice and video calling** support
- **Group chat** and friend management
- **File upload/download** with progress tracking
- Support for **iOS 13+** and **macOS 10.15+**

## Installation

### CocoaPods

Add to your `Podfile`:

```ruby
platform :ios, '13.0'
use_frameworks!

target 'YourApp' do
  pod 'AnyChatSDK', '~> 0.1'
end
```

Then run:

```bash
pod install
```

### Swift Package Manager

Add to your `Package.swift`:

```swift
dependencies: [
    .package(url: "https://github.com/yzhgit/anychat-sdk.git", from: "0.1.0")
]
```

Or add via Xcode:
1. File > Add Packages...
2. Enter: `https://github.com/yzhgit/anychat-sdk.git`
3. Select version: 0.1.0 or later

## Quick Start

### Initialize the Client

```swift
import AnyChatSDK

let config = ClientConfig(
    gatewayURL: "wss://api.anychat.io",
    apiBaseURL: "https://api.anychat.io/api/v1",
    deviceId: UIDevice.current.identifierForVendor?.uuidString ?? "unknown",
    dbPath: NSSearchPathForDirectoriesInDomains(.documentDirectory, .userDomainMask, true)[0] + "/anychat.db"
)

let client = try AnyChatClient(config: config)
client.connect()
```

### Authentication

```swift
// Login
do {
    let token = try await client.auth.login(
        account: "user@example.com",
        password: "password123"
    )
    print("Logged in with token: \(token.accessToken)")
} catch {
    print("Login failed: \(error)")
}

// Listen for token expiration
Task {
    for await _ in client.auth.tokenExpired {
        print("Token expired, need to re-authenticate")
    }
}
```

### Messaging

```swift
// Send a text message
try await client.message.sendText(
    sessionId: "conv_123",
    content: "Hello, World!"
)

// Get message history
let messages = try await client.message.getHistory(
    sessionId: "conv_123",
    limit: 50
)

// Listen for incoming messages
Task {
    for await message in client.message.receivedMessages {
        print("New message from \(message.senderId): \(message.content)")
    }
}
```

### Voice/Video Calls

```swift
// Initiate a video call
let session = try await client.call.initiateCall(
    calleeId: "user_456",
    callType: .video
)

// Join incoming call
try await client.call.joinCall(callId: session.callId)

// Listen for incoming calls
Task {
    for await session in client.call.incomingCall {
        print("Incoming \(session.callType == .video ? "video" : "audio") call from \(session.callerId)")
    }
}
```

### Conversations

```swift
// Get conversation list
let conversations = try await client.conversation.getList()

// Mark as read
try await client.conversation.markRead(conversationId: "conv_123")

// Pin/unpin conversation
try await client.conversation.setPinned(conversationId: "conv_123", pinned: true)
```

### Friends

```swift
// Get friend list
let friends = try await client.friend.getList()

// Send friend request
try await client.friend.sendRequest(
    toUserId: "user_456",
    message: "Let's be friends!"
)

// Handle friend request
try await client.friend.handleRequest(
    requestId: 123,
    accept: true
)
```

### Groups

```swift
// Get group list
let groups = try await client.group.getList()

// Create a group
try await client.group.create(
    name: "My Group",
    memberIds: ["user_1", "user_2", "user_3"]
)

// Invite users to group
try await client.group.invite(
    groupId: "group_789",
    userIds: ["user_4", "user_5"]
)
```

### File Upload/Download

```swift
// Upload a file with progress
let fileInfo = try await client.file.upload(
    localPath: "/path/to/image.jpg",
    fileType: "image"
) { uploaded, total in
    let progress = Double(uploaded) / Double(total) * 100
    print("Upload progress: \(progress)%")
}

// Get download URL
let downloadURL = try await client.file.getDownloadURL(fileId: fileInfo.fileId)
```

## Error Handling

All async methods can throw `AnyChatError`:

```swift
do {
    try await client.auth.login(account: "user", password: "pass")
} catch let error as AnyChatError {
    switch error {
    case .auth:
        print("Authentication failed")
    case .network:
        print("Network error")
    case .timeout:
        print("Request timed out")
    case .notFound:
        print("Resource not found")
    case .tokenExpired:
        print("Token expired")
    default:
        print("Error: \(error)")
    }
}
```

## Requirements

- iOS 13.0+ / macOS 10.15+
- Swift 5.9+
- Xcode 15.0+

## Architecture

The SDK uses modern Swift concurrency features:

- **Actor-based design**: All managers are actors, ensuring thread-safety
- **async/await**: All asynchronous operations use async/await
- **AsyncStream**: Event streams use AsyncStream for reactive programming
- **Automatic memory management**: C resources are automatically freed

## Documentation

- [API Documentation](https://yzhgit.github.io/anychat-server)
- [Backend Repository](https://github.com/yzhgit/anychat-server)
- [Example App](Example/)

## Example App

See the [Example](Example/) directory for a complete iOS app demonstrating SDK usage.

To run the example:

```bash
cd Example
pod install
open AnyChatSDKExample.xcworkspace
```

## Support

- **Issues**: [GitHub Issues](https://github.com/yzhgit/anychat-sdk/issues)
- **Email**: sdk@anychat.io
- **Documentation**: https://yzhgit.github.io/anychat-server

## License

AnyChatSDK is available under the MIT license. See the [LICENSE](../../LICENSE) file for more info.

## Contributing

We welcome contributions! Please feel free to submit a Pull Request.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for a list of changes.
