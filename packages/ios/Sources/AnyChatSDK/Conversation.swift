//
//  Conversation.swift
//  AnyChatSDK
//
//  Conversation manager with async/await support
//

import Foundation

public actor ConversationManager {
    private let handle: AnyChatConvHandle
    private var updatedContinuation: AsyncStream<Conversation>.Continuation?

    init(handle: AnyChatConvHandle) {
        self.handle = handle
        setupCallbacks()
    }

    // MARK: - Conversation Operations

    public func getConversationList() async throws -> [Conversation] {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatConvListCallback = { userdata, list, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<[Conversation]>>.fromOpaque(userdata).takeRetainedValue()

                if let list = list {
                    let conversations = convertConversationList(list)
                    context.continuation.resume(returning: conversations)
                    var mutableList = list.pointee
                    mutableList.free()
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Failed to fetch conversations"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            let result = anychat_conv_get_list(handle, userdata, callback)

            if result != ANYCHAT_OK {
                let ctx = Unmanaged<CallbackContext<[Conversation]>>.fromOpaque(userdata).takeRetainedValue()
                ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
            }
        }
    }

    public func setPinned(
        conversationId: String,
        pinned: Bool
    ) async throws {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatConvCallback = { userdata, success, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Set pinned failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(conversationId) { convPtr in
                let result = anychat_conv_set_pinned(
                    handle,
                    convPtr,
                    pinned ? 1 : 0,
                    userdata,
                    callback
                )

                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    public func setMuted(
        conversationId: String,
        muted: Bool
    ) async throws {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatConvCallback = { userdata, success, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Set muted failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(conversationId) { convPtr in
                let result = anychat_conv_set_muted(
                    handle,
                    convPtr,
                    muted ? 1 : 0,
                    userdata,
                    callback
                )

                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    public func delete(conversationId: String) async throws {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatConvCallback = { userdata, success, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Delete failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(conversationId) { convPtr in
                let result = anychat_conv_delete(
                    handle,
                    convPtr,
                    userdata,
                    callback
                )

                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    // MARK: - Event Streams

    public var conversationUpdated: AsyncStream<Conversation> {
        AsyncStream { continuation in
            self.updatedContinuation = continuation
        }
    }

    // MARK: - Private

    private func setupCallbacks() {
        let callback: AnyChatConvUpdatedCallback = { userdata, conversation in
            guard let userdata = userdata, let conversation = conversation else { return }
            let context = Unmanaged<StreamContext<Conversation>>.fromOpaque(userdata).takeUnretainedValue()
            context.continuation.yield(Conversation(from: conversation.pointee))
        }

        Task {
            let context = StreamContext(continuation: updatedContinuation!)
            let userdata = Unmanaged.passRetained(context).toOpaque()
            anychat_conv_set_updated_callback(handle, userdata, callback)
        }
    }
}
