//
//  AnyChatClient.swift
//  AnyChatSDK
//
//  Main entry point for the AnyChatSDK
//

import Foundation

public actor AnyChatClient {
    private let handleWrapper: ClientHandleWrapper
    private var connectionStateContinuation: AsyncStream<ConnectionState>.Continuation?

    public let auth: AuthManager
    public let message: MessageManager
    public let conversation: ConversationManager

    // MARK: - Initialization

    public init(config: ClientConfig) throws {
        // Create C config
        var cConfig = AnyChatClientConfig_C()

        // Store strings temporarily to keep them alive during the C call
        let gatewayURL = config.gatewayURL
        let apiBaseURL = config.apiBaseURL
        let deviceId = config.deviceId
        let dbPath = config.dbPath

        cConfig.gateway_url = nil
        cConfig.api_base_url = nil
        cConfig.device_id = nil
        cConfig.db_path = nil
        cConfig.connect_timeout_ms = Int32(config.connectTimeoutMs)
        cConfig.max_reconnect_attempts = Int32(config.maxReconnectAttempts)
        cConfig.auto_reconnect = config.autoReconnect ? 1 : 0

        // Create client handle with proper string management
        let handle = withCString(gatewayURL) { gatewayPtr in
            withCString(apiBaseURL) { apiPtr in
                withCString(deviceId) { devicePtr in
                    withCString(dbPath) { dbPtr in
                        var config = cConfig
                        config.gateway_url = gatewayPtr
                        config.api_base_url = apiPtr
                        config.device_id = devicePtr
                        config.db_path = dbPtr
                        return anychat_client_create(&config)
                    }
                }
            }
        }

        guard let handle = handle else {
            let errorMsg = getLastError()
            throw AnyChatError.unknown(code: -1, message: errorMsg)
        }

        self.handleWrapper = ClientHandleWrapper(handle: handle)

        // Get sub-module handles
        let authHandle = anychat_client_get_auth(handle)
        let messageHandle = anychat_client_get_message(handle)
        let convHandle = anychat_client_get_conversation(handle)

        self.auth = AuthManager(handle: authHandle!)
        self.message = MessageManager(handle: messageHandle!)
        self.conversation = ConversationManager(handle: convHandle!)

        setupConnectionCallback()
    }

    deinit {
        // Cleanup is handled by ClientHandleWrapper
    }

    // MARK: - Connection Management

    public func connect() {
        anychat_client_connect(handleWrapper.handle)
    }

    public func disconnect() {
        anychat_client_disconnect(handleWrapper.handle)
    }

    public func getConnectionState() -> ConnectionState {
        let state = anychat_client_get_connection_state(handleWrapper.handle)
        return ConnectionState(rawValue: Int(state)) ?? .disconnected
    }

    // MARK: - Event Streams

    public var connectionState: AsyncStream<ConnectionState> {
        AsyncStream { continuation in
            self.connectionStateContinuation = continuation
        }
    }

    // MARK: - Private

    private func setupConnectionCallback() {
        let callback: AnyChatConnectionStateCallback = { userdata, state in
            guard let userdata = userdata else { return }
            let context = Unmanaged<StreamContext<ConnectionState>>.fromOpaque(userdata).takeUnretainedValue()
            if let connectionState = ConnectionState(rawValue: Int(state)) {
                context.continuation.yield(connectionState)
            }
        }

        Task {
            let context = StreamContext(continuation: connectionStateContinuation!)
            let userdata = Unmanaged.passRetained(context).toOpaque()
            anychat_client_set_connection_callback(
                handleWrapper.handle,
                userdata,
                callback
            )
        }
    }
}

// MARK: - Manager Access Extension
extension AnyChatClient {
    public var friend: FriendManager {
        let handle = anychat_client_get_friend(handleWrapper.handle)
        return FriendManager(handle: handle!)
    }

    public var group: GroupManager {
        let handle = anychat_client_get_group(handleWrapper.handle)
        return GroupManager(handle: handle!)
    }

    public var user: UserManager {
        let handle = anychat_client_get_user(handleWrapper.handle)
        return UserManager(handle: handle!)
    }

    public var file: FileManager {
        let handle = anychat_client_get_file(handleWrapper.handle)
        return FileManager(handle: handle!)
    }

    public var call: CallManager {
        let handle = anychat_client_get_call(handleWrapper.handle)
        return CallManager(handle: handle!)
    }
}
