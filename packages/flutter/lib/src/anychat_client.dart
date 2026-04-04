import 'dart:async';
import 'dart:ffi';
import 'package:ffi/ffi.dart';

import 'anychat_ffi_bindings.dart';
import 'native_loader.dart';
import 'models.dart';

// Global callback context management
// Maps callback ID to Dart completer and NativeCallable for async operations
class _CallbackContext {
  final dynamic completer;
  final NativeCallable? callable;

  _CallbackContext(this.completer, [this.callable]);
}

final _callbackContexts = <int, _CallbackContext>{};
int _nextCallbackId = 1;

int _registerCallback(dynamic completer, [NativeCallable? callable]) {
  final id = _nextCallbackId++;
  _callbackContexts[id] = _CallbackContext(completer, callable);
  return id;
}

void _unregisterCallback(int id) {
  final context = _callbackContexts.remove(id);
  context?.callable?.close();
}

dynamic _getCallback(int id) {
  return _callbackContexts[id]?.completer;
}

// ── Static callback functions for C FFI ─────────────────────────────────────

// Auth callback: used for login, register, refreshToken
void _authCallbackNative(
  Pointer<Void> userdata,
  int success,
  Pointer<AnyChatAuthToken_C> token,
  Pointer<Char> error,
) {
  final id = userdata.address;
  final completer = _getCallback(id) as Completer<AuthToken>?;
  if (completer == null) return;

  if (success != 0 && token != nullptr) {
    final dartToken = AuthToken(
      accessToken: _copyFixedStringStatic(token.ref.access_token, 512),
      refreshToken: _copyFixedStringStatic(token.ref.refresh_token, 512),
      expiresAtMs: token.ref.expires_at_ms,
    );
    completer.complete(dartToken);
  } else {
    String errorMsg = 'Unknown error';
    if (error != nullptr) {
      try {
        errorMsg = error.cast<Utf8>().toDartString();
      } catch (e) {
        // If UTF-8 decoding fails, try to read as much as possible
        errorMsg = 'Error (encoding issue): ${e.toString()}';
      }
    }
    completer.completeError(Exception(errorMsg));
  }
  _unregisterCallback(id);
}

// Result callback: used for logout, changePassword, etc.
void _resultCallbackNative(
  Pointer<Void> userdata,
  int success,
  Pointer<Char> error,
) {
  final id = userdata.address;
  final completer = _getCallback(id) as Completer<void>?;
  if (completer == null) return;

  if (success != 0) {
    completer.complete();
  } else {
    String errorMsg = 'Unknown error';
    if (error != nullptr) {
      try {
        errorMsg = error.cast<Utf8>().toDartString();
      } catch (e) {
        errorMsg = 'Error (encoding issue): ${e.toString()}';
      }
    }
    completer.completeError(Exception(errorMsg));
  }
  _unregisterCallback(id);
}

// Connection state callback
void _connectionStateCallbackNative(
  Pointer<Void> userdata,
  int state,
) {
  final client = _getCallback(userdata.address) as AnyChatClient?;
  if (client == null) return;

  final connectionState = ConnectionState.fromInt(state);
  client._connectionStateController.add(connectionState);
}

// Message callback: used for sendTextMessage, markRead
void _messageCallbackNative(
  Pointer<Void> userdata,
  int success,
  Pointer<Char> error,
) {
  final id = userdata.address;
  final completer = _getCallback(id) as Completer<void>?;
  if (completer == null) return;

  if (success != 0) {
    completer.complete();
  } else {
    String errorMsg = 'Unknown error';
    if (error != nullptr) {
      try {
        errorMsg = error.cast<Utf8>().toDartString();
      } catch (e) {
        errorMsg = 'Error (encoding issue): ${e.toString()}';
      }
    }
    completer.completeError(Exception(errorMsg));
  }
  _unregisterCallback(id);
}

// Message list callback: used for getMessageHistory
void _messageListCallbackNative(
  Pointer<Void> userdata,
  Pointer<AnyChatMessageList_C> list,
  Pointer<Char> error,
) {
  final id = userdata.address;
  final completer = _getCallback(id) as Completer<List<Message>>?;
  if (completer == null) return;

  if (error != nullptr) {
    String errorMsg = 'Unknown error';
    try {
      errorMsg = error.cast<Utf8>().toDartString();
    } catch (e) {
      errorMsg = 'Error (encoding issue): ${e.toString()}';
    }
    completer.completeError(Exception(errorMsg));
  } else if (list != nullptr) {
    final messages = <Message>[];
    final count = list.ref.count;
    for (var i = 0; i < count; i++) {
      final item = list.ref.items.elementAt(i).ref;
      messages.add(Message(
        messageId: _copyFixedStringStatic(item.message_id, 64),
        localId: _copyFixedStringStatic(item.local_id, 64),
        convId: _copyFixedStringStatic(item.conv_id, 64),
        senderId: _copyFixedStringStatic(item.sender_id, 64),
        contentType: _copyFixedStringStatic(item.content_type, 32),
        type: MessageType.fromInt(item.type),
        content: item.content.cast<Utf8>().toDartString(),
        seq: item.seq,
        replyTo: _copyFixedStringStatic(item.reply_to, 64),
        timestampMs: item.timestamp_ms,
        status: item.status,
        sendState: SendState.fromInt(item.send_state),
        isRead: item.is_read != 0,
      ));
    }
    completer.complete(messages);
  } else {
    completer.complete([]);
  }
  _unregisterCallback(id);
}

// Conversation list callback
void _convListCallbackNative(
  Pointer<Void> userdata,
  Pointer<AnyChatConversationList_C> list,
  Pointer<Char> error,
) {
  final id = userdata.address;
  final completer = _getCallback(id) as Completer<List<Conversation>>?;
  if (completer == null) return;

  if (error != nullptr) {
    String errorMsg = 'Unknown error';
    try {
      errorMsg = error.cast<Utf8>().toDartString();
    } catch (e) {
      errorMsg = 'Error (encoding issue): ${e.toString()}';
    }
    completer.completeError(Exception(errorMsg));
  } else if (list != nullptr) {
    final conversations = <Conversation>[];
    final count = list.ref.count;
    for (var i = 0; i < count; i++) {
      final item = list.ref.items.elementAt(i).ref;
      conversations.add(Conversation(
        convId: _copyFixedStringStatic(item.conv_id, 64),
        convType: ConversationType.fromInt(item.conv_type),
        targetId: _copyFixedStringStatic(item.target_id, 64),
        lastMsgId: _copyFixedStringStatic(item.last_msg_id, 64),
        lastMsgText: _copyFixedStringStatic(item.last_msg_text, 512),
        lastMsgTimeMs: item.last_msg_time_ms,
        unreadCount: item.unread_count,
        isPinned: item.is_pinned != 0,
        isMuted: item.is_muted != 0,
        updatedAtMs: item.updated_at_ms,
      ));
    }
    completer.complete(conversations);
  } else {
    completer.complete([]);
  }
  _unregisterCallback(id);
}

// Conversation callback: used for markConversationRead
void _convCallbackNative(
  Pointer<Void> userdata,
  int success,
  Pointer<Char> error,
) {
  final id = userdata.address;
  final completer = _getCallback(id) as Completer<void>?;
  if (completer == null) return;

  if (success != 0) {
    completer.complete();
  } else {
    String errorMsg = 'Unknown error';
    if (error != nullptr) {
      try {
        errorMsg = error.cast<Utf8>().toDartString();
      } catch (e) {
        errorMsg = 'Error (encoding issue): ${e.toString()}';
      }
    }
    completer.completeError(Exception(errorMsg));
  }
  _unregisterCallback(id);
}

// Friend list callback
void _friendListCallbackNative(
  Pointer<Void> userdata,
  Pointer<AnyChatFriendList_C> list,
  Pointer<Char> error,
) {
  final id = userdata.address;
  final completer = _getCallback(id) as Completer<List<Friend>>?;
  if (completer == null) return;

  if (error != nullptr) {
    String errorMsg = 'Unknown error';
    try {
      errorMsg = error.cast<Utf8>().toDartString();
    } catch (e) {
      errorMsg = 'Error (encoding issue): ${e.toString()}';
    }
    completer.completeError(Exception(errorMsg));
  } else if (list != nullptr) {
    final friends = <Friend>[];
    final count = list.ref.count;
    for (var i = 0; i < count; i++) {
      final item = list.ref.items.elementAt(i).ref;
      friends.add(Friend(
        userId: _copyFixedStringStatic(item.user_id, 64),
        remark: _copyFixedStringStatic(item.remark, 128),
        updatedAtMs: item.updated_at_ms,
        isDeleted: item.is_deleted != 0,
        userInfo: UserInfo(
          userId: _copyFixedStringStatic(item.user_info.user_id, 64),
          username: _copyFixedStringStatic(item.user_info.username, 128),
          avatarUrl: _copyFixedStringStatic(item.user_info.avatar_url, 512),
        ),
      ));
    }
    completer.complete(friends);
  } else {
    completer.complete([]);
  }
  _unregisterCallback(id);
}

// Helper: Copy fixed-size C string array to Dart String (static version)
String _copyFixedStringStatic(Array<Char> arr, int maxLength) {
  final units = <int>[];
  for (var i = 0; i < maxLength; i++) {
    final char = arr[i];
    if (char == 0) break;
    units.add(char);
  }
  return String.fromCharCodes(units);
}

/// Main AnyChat client for Flutter.
///
/// This is a high-level Dart wrapper around the C FFI bindings.
/// All async operations return Future<T> instead of C callbacks.
class AnyChatClient {
  late final AnyChatNativeBindings _bindings;
  late final Pointer<AnyChatClient_T> _clientHandle;

  // Sub-module handles (cached)
  Pointer<AnyChatAuthManager_T>? _authHandle;
  Pointer<AnyChatMessage_T>? _messageHandle;
  Pointer<AnyChatConversation_T>? _convHandle;
  Pointer<AnyChatFriend_T>? _friendHandle;
  Pointer<AnyChatGroup_T>? _groupHandle;
  Pointer<AnyChatFile_T>? _fileHandle;
  Pointer<AnyChatUser_T>? _userHandle;
  Pointer<AnyChatRtc_T>? _rtcHandle;

  // Connection state stream
  final _connectionStateController = StreamController<ConnectionState>.broadcast();
  Stream<ConnectionState> get connectionStateStream => _connectionStateController.stream;

  // Message received stream
  final _messageReceivedController = StreamController<Message>.broadcast();
  Stream<Message> get messageReceivedStream => _messageReceivedController.stream;

  // Conversation updated stream
  final _conversationUpdatedController = StreamController<Conversation>.broadcast();
  Stream<Conversation> get conversationUpdatedStream => _conversationUpdatedController.stream;

  // Friend request stream
  final _friendRequestController = StreamController<FriendRequest>.broadcast();
  Stream<FriendRequest> get friendRequestStream => _friendRequestController.stream;

  // Incoming call stream
  final _incomingCallController = StreamController<CallSession>.broadcast();
  Stream<CallSession> get incomingCallStream => _incomingCallController.stream;

  /// Creates a new AnyChat client with the given configuration.
  AnyChatClient({
    required String gatewayUrl,
    required String apiBaseUrl,
    required String deviceId,
    String? dbPath,
    int connectTimeoutMs = 10000,
    int maxReconnectAttempts = 5,
    bool autoReconnect = true,
  }) {
    _bindings = AnyChatNativeBindings(NativeLibraryLoader.library);

    // Allocate and populate the config struct
    final config = calloc<AnyChatClientConfig_C>();
    config.ref.gateway_url = gatewayUrl.toNativeUtf8().cast();
    config.ref.api_base_url = apiBaseUrl.toNativeUtf8().cast();
    config.ref.device_id = deviceId.toNativeUtf8().cast();
    config.ref.db_path = dbPath != null ? dbPath.toNativeUtf8().cast() : nullptr;
    config.ref.connect_timeout_ms = connectTimeoutMs;
    config.ref.max_reconnect_attempts = maxReconnectAttempts;
    config.ref.auto_reconnect = autoReconnect ? 1 : 0;

    // Create the client
    _clientHandle = _bindings.anychat_client_create(config);

    // Free the config strings
    calloc.free(config.ref.gateway_url);
    calloc.free(config.ref.api_base_url);
    calloc.free(config.ref.device_id);
    if (dbPath != null) calloc.free(config.ref.db_path);
    calloc.free(config);

    if (_clientHandle == nullptr) {
      final errorPtr = _bindings.anychat_get_last_error();
      String error = 'Unknown error';
      try {
        error = errorPtr.cast<Utf8>().toDartString();
      } catch (e) {
        error = 'Error creating client (encoding issue)';
      }
      throw Exception('Failed to create client: $error');
    }

    // Setup connection state callback
    _setupConnectionStateCallback();
  }

  // Keep NativeCallable alive for connection state callback
  late final NativeCallable<Void Function(Pointer<Void>, Int)> _connectionStateCallableNative;

  void _setupConnectionStateCallback() {
    // Register connection state callback with client instance as userdata
    final clientId = _registerCallback(this);

    // Create listener callable for multi-invocation callback from external thread
    _connectionStateCallableNative = NativeCallable<
        Void Function(Pointer<Void>, Int)>.listener(
      _connectionStateCallbackNative,
    );

    _bindings.anychat_client_set_connection_callback(
      _clientHandle,
      Pointer<Void>.fromAddress(clientId),
      _connectionStateCallableNative.nativeFunction,
    );
  }

  /// Gets the current connection state.
  ConnectionState get connectionState {
    final state = _bindings.anychat_client_get_connection_state(_clientHandle);
    return ConnectionState.fromInt(state);
  }

  /// Disposes the client and releases all resources.
  void dispose() {
    _bindings.anychat_client_destroy(_clientHandle);
    _connectionStateCallableNative.close();
    _connectionStateController.close();
    _messageReceivedController.close();
    _conversationUpdatedController.close();
    _friendRequestController.close();
    _incomingCallController.close();
  }

  // ── Auth module ──────────────────────────────────────────────────────────────

  Pointer<AnyChatAuthManager_T> get _auth {
    _authHandle ??= _bindings.anychat_client_get_auth(_clientHandle);
    return _authHandle!;
  }

  /// Logs in with account and password, then automatically establishes WebSocket connection.
  ///
  /// The WebSocket connection is managed automatically by the SDK. Auto-reconnection
  /// is handled internally when network interruptions occur.
  Future<AuthToken> login({
    required String account,
    required String password,
    String deviceType = 'PC',
    String clientVersion = '',
  }) {
    final completer = Completer<AuthToken>();

    // Create listener callable for callback from external thread
    final callable = NativeCallable<
        Void Function(
          Pointer<Void>,
          Int,
          Pointer<AnyChatAuthToken_C>,
          Pointer<Char>,
        )>.listener(_authCallbackNative);

    final callbackId = _registerCallback(completer, callable);

    final accountPtr = account.toNativeUtf8();
    final passwordPtr = password.toNativeUtf8();
    final deviceTypePtr = deviceType.toNativeUtf8();
    final clientVersionPtr = clientVersion.toNativeUtf8();

    final ret = _bindings.anychat_client_login(
      _clientHandle,
      accountPtr.cast(),
      passwordPtr.cast(),
      deviceTypePtr.cast(),
      clientVersionPtr.cast(),
      Pointer<Void>.fromAddress(callbackId),
      callable.nativeFunction,
    );

    calloc.free(accountPtr);
    calloc.free(passwordPtr);
    calloc.free(deviceTypePtr);
    calloc.free(clientVersionPtr);

    if (ret != 0) {
      _unregisterCallback(callbackId);
      final errorPtr = _bindings.anychat_get_last_error();
      String error = 'Unknown error';
      try {
        error = errorPtr.cast<Utf8>().toDartString();
      } catch (e) {
        error = 'Encoding issue';
      }
      completer.completeError(Exception('Failed to dispatch login request: $error'));
    }

    return completer.future;
  }

  /// Registers a new user account.
  ///
  /// [phoneOrEmail] - Phone number or email address
  /// [password] - Account password
  /// [verifyCode] - SMS or email verification code
  /// [deviceType] - Device type: iOS/Android/Web/PC
  /// [nickname] - Optional user nickname (pass empty string to skip)
  /// [clientVersion] - Client version string (e.g. "1.0.0")
  Future<AuthToken> register({
    required String phoneOrEmail,
    required String password,
    required String verifyCode,
    String deviceType = 'PC',
    String nickname = '',
    String clientVersion = '',
  }) {
    final completer = Completer<AuthToken>();

    // Create listener callable for callback from external thread
    final callable = NativeCallable<
        Void Function(
          Pointer<Void>,
          Int,
          Pointer<AnyChatAuthToken_C>,
          Pointer<Char>,
        )>.listener(_authCallbackNative);

    final callbackId = _registerCallback(completer, callable);

    final phoneOrEmailPtr = phoneOrEmail.toNativeUtf8();
    final passwordPtr = password.toNativeUtf8();
    final verifyCodePtr = verifyCode.toNativeUtf8();
    final deviceTypePtr = deviceType.toNativeUtf8();
    final nicknamePtr = nickname.toNativeUtf8();
    final clientVersionPtr = clientVersion.toNativeUtf8();

    final ret = _bindings.anychat_auth_register(
      _auth,
      phoneOrEmailPtr.cast(),
      passwordPtr.cast(),
      verifyCodePtr.cast(),
      deviceTypePtr.cast(),
      nicknamePtr.cast(),
      clientVersionPtr.cast(),
      Pointer<Void>.fromAddress(callbackId),
      callable.nativeFunction,
    );

    calloc.free(phoneOrEmailPtr);
    calloc.free(passwordPtr);
    calloc.free(verifyCodePtr);
    calloc.free(deviceTypePtr);
    calloc.free(nicknamePtr);
    calloc.free(clientVersionPtr);

    if (ret != 0) {
      _unregisterCallback(callbackId);
      final errorPtr = _bindings.anychat_get_last_error();
      String error = 'Unknown error';
      try {
        error = errorPtr.cast<Utf8>().toDartString();
      } catch (e) {
        error = 'Encoding issue';
      }
      completer.completeError(Exception('Failed to dispatch register request: $error'));
    }

    return completer.future;
  }

  /// Checks if the user is logged in.
  bool get isLoggedIn {
    return _bindings.anychat_client_is_logged_in(_clientHandle) != 0;
  }

  /// Gets the current auth token (if logged in).
  AuthToken? get currentToken {
    if (!isLoggedIn) return null;

    final tokenStruct = calloc<AnyChatAuthToken_C>();
    final ret = _bindings.anychat_client_get_current_token(_clientHandle, tokenStruct);

    if (ret != 0) {
      calloc.free(tokenStruct);
      return null;
    }

    final token = AuthToken(
      accessToken: _copyFixedString(tokenStruct.ref.access_token, 512),
      refreshToken: _copyFixedString(tokenStruct.ref.refresh_token, 512),
      expiresAtMs: tokenStruct.ref.expires_at_ms,
    );

    calloc.free(tokenStruct);
    return token;
  }

  /// Logs out: disconnects WebSocket and calls HTTP logout endpoint.
  Future<void> logout() {
    final completer = Completer<void>();

    // Create listener callable
    final callable = NativeCallable<
        Void Function(Pointer<Void>, Int, Pointer<Char>)>.listener(
      _resultCallbackNative,
    );

    final callbackId = _registerCallback(completer, callable);

    final ret = _bindings.anychat_client_logout(
      _clientHandle,
      Pointer<Void>.fromAddress(callbackId),
      callable.nativeFunction,
    );

    if (ret != 0) {
      _unregisterCallback(callbackId);
      final errorPtr = _bindings.anychat_get_last_error();
      String error = 'Unknown error';
      try {
        error = errorPtr.cast<Utf8>().toDartString();
      } catch (e) {
        error = 'Encoding issue';
      }
      completer.completeError(Exception('Failed to dispatch logout request: $error'));
    }

    return completer.future;
  }

  // ── Message module ───────────────────────────────────────────────────────────

  Pointer<AnyChatMessage_T> get _message {
    _messageHandle ??= _bindings.anychat_client_get_message(_clientHandle);
    return _messageHandle!;
  }

  /// Sends a text message to a conversation.
  Future<void> sendTextMessage({
    required String sessionId,
    required String content,
  }) {
    final completer = Completer<void>();

    // Create listener callable
    final callable = NativeCallable<
        Void Function(Pointer<Void>, Int, Pointer<Char>)>.listener(
      _messageCallbackNative,
    );

    final callbackId = _registerCallback(completer, callable);

    final sessionIdPtr = sessionId.toNativeUtf8();
    final contentPtr = content.toNativeUtf8();

    final ret = _bindings.anychat_message_send_text(
      _message,
      sessionIdPtr.cast(),
      contentPtr.cast(),
      Pointer<Void>.fromAddress(callbackId),
      callable.nativeFunction,
    );

    calloc.free(sessionIdPtr);
    calloc.free(contentPtr);

    if (ret != 0) {
      _unregisterCallback(callbackId);
      final errorPtr = _bindings.anychat_get_last_error();
      String error = 'Unknown error';
      try {
        error = errorPtr.cast<Utf8>().toDartString();
      } catch (e) {
        error = 'Encoding issue';
      }
      completer.completeError(Exception('Failed to send message: $error'));
    }

    return completer.future;
  }

  /// Fetches message history for a conversation.
  Future<List<Message>> getMessageHistory({
    required String sessionId,
    int beforeTimestampMs = 0,
    int limit = 20,
  }) {
    final completer = Completer<List<Message>>();

    // Create listener callable
    final callable = NativeCallable<
        Void Function(
          Pointer<Void>,
          Pointer<AnyChatMessageList_C>,
          Pointer<Char>,
        )>.listener(_messageListCallbackNative);

    final callbackId = _registerCallback(completer, callable);

    final sessionIdPtr = sessionId.toNativeUtf8();

    final ret = _bindings.anychat_message_get_history(
      _message,
      sessionIdPtr.cast(),
      beforeTimestampMs,
      limit,
      Pointer<Void>.fromAddress(callbackId),
      callable.nativeFunction,
    );

    calloc.free(sessionIdPtr);

    if (ret != 0) {
      _unregisterCallback(callbackId);
      final errorPtr = _bindings.anychat_get_last_error();
      String error = 'Unknown error';
      try {
        error = errorPtr.cast<Utf8>().toDartString();
      } catch (e) {
        error = 'Encoding issue';
      }
      completer.completeError(Exception('Failed to get message history: $error'));
    }

    return completer.future;
  }

  // ── Conversation module ──────────────────────────────────────────────────────

  Pointer<AnyChatConversation_T> get _conv {
    _convHandle ??= _bindings.anychat_client_get_conversation(_clientHandle);
    return _convHandle!;
  }

  /// Gets the list of conversations.
  Future<List<Conversation>> getConversations() {
    final completer = Completer<List<Conversation>>();

    // Create listener callable
    final callable = NativeCallable<
        Void Function(
          Pointer<Void>,
          Pointer<AnyChatConversationList_C>,
          Pointer<Char>,
        )>.listener(_convListCallbackNative);

    final callbackId = _registerCallback(completer, callable);

    final ret = _bindings.anychat_conv_get_list(
      _conv,
      Pointer<Void>.fromAddress(callbackId),
      callable.nativeFunction,
    );

    if (ret != 0) {
      _unregisterCallback(callbackId);
      final errorPtr = _bindings.anychat_get_last_error();
      String error = 'Unknown error';
      try {
        error = errorPtr.cast<Utf8>().toDartString();
      } catch (e) {
        error = 'Encoding issue';
      }
      completer.completeError(Exception('Failed to get conversations: $error'));
    }

    return completer.future;
  }

  /// Marks a conversation as read.
  Future<void> markConversationRead(String convId) {
    final completer = Completer<void>();

    // Create listener callable
    final callable = NativeCallable<
        Void Function(Pointer<Void>, Int, Pointer<Char>)>.listener(
      _convCallbackNative,
    );

    final callbackId = _registerCallback(completer, callable);

    final convIdPtr = convId.toNativeUtf8();

    final ret = _bindings.anychat_conv_mark_read(
      _conv,
      convIdPtr.cast(),
      Pointer<Void>.fromAddress(callbackId),
      callable.nativeFunction,
    );

    calloc.free(convIdPtr);

    if (ret != 0) {
      _unregisterCallback(callbackId);
      final errorPtr = _bindings.anychat_get_last_error();
      String error = 'Unknown error';
      try {
        error = errorPtr.cast<Utf8>().toDartString();
      } catch (e) {
        error = 'Encoding issue';
      }
      completer.completeError(Exception('Failed to mark conversation as read: $error'));
    }

    return completer.future;
  }

  // ── Friend module ────────────────────────────────────────────────────────────

  Pointer<AnyChatFriend_T> get _friend {
    _friendHandle ??= _bindings.anychat_client_get_friend(_clientHandle);
    return _friendHandle!;
  }

  /// Gets the friend list.
  Future<List<Friend>> getFriends() {
    final completer = Completer<List<Friend>>();

    // Create listener callable
    final callable = NativeCallable<
        Void Function(
          Pointer<Void>,
          Pointer<AnyChatFriendList_C>,
          Pointer<Char>,
        )>.listener(_friendListCallbackNative);

    final callbackId = _registerCallback(completer, callable);

    final ret = _bindings.anychat_friend_get_list(
      _friend,
      Pointer<Void>.fromAddress(callbackId),
      callable.nativeFunction,
    );

    if (ret != 0) {
      _unregisterCallback(callbackId);
      final errorPtr = _bindings.anychat_get_last_error();
      String error = 'Unknown error';
      try {
        error = errorPtr.cast<Utf8>().toDartString();
      } catch (e) {
        error = 'Encoding issue';
      }
      completer.completeError(Exception('Failed to get friends: $error'));
    }

    return completer.future;
  }

  // ── Helper methods ───────────────────────────────────────────────────────────

  /// Copies a fixed-size C string array to a Dart String.
  String _copyFixedString(Array<Char> arr, int maxLength) {
    final units = <int>[];
    for (var i = 0; i < maxLength; i++) {
      final char = arr[i];
      if (char == 0) break;
      units.add(char);
    }
    return String.fromCharCodes(units);
  }
}
