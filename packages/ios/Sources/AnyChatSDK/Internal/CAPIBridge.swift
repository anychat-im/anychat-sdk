//
//  CAPIBridge.swift
//  AnyChatSDK
//
//  Low-level C API wrappers with proper memory management
//

import Foundation

// MARK: - Callback Context

final class CallbackContext<T: Sendable>: @unchecked Sendable {
    let continuation: CheckedContinuation<T, Error>

    init(continuation: CheckedContinuation<T, Error>) {
        self.continuation = continuation
    }
}

// MARK: - Stream Context

final class StreamContext<T: Sendable>: @unchecked Sendable {
    let continuation: AsyncStream<T>.Continuation

    init(continuation: AsyncStream<T>.Continuation) {
        self.continuation = continuation
    }

    deinit {
        continuation.finish()
    }
}

// MARK: - Client Handle Wrapper

final class ClientHandleWrapper: @unchecked Sendable {
    let handle: AnyChatClientHandle

    init(handle: AnyChatClientHandle) {
        self.handle = handle
    }

    deinit {
        anychat_client_destroy(handle)
    }
}

// MARK: - Memory Management Extensions

extension AnyChatMessage_C {
    mutating func free() {
        anychat_free_message(&self)
    }
}

extension AnyChatMessageList_C {
    mutating func free() {
        anychat_free_message_list(&self)
    }
}

extension AnyChatConversationList_C {
    mutating func free() {
        anychat_free_conversation_list(&self)
    }
}

extension AnyChatFriendList_C {
    mutating func free() {
        anychat_free_friend_list(&self)
    }
}

extension AnyChatFriendRequestList_C {
    mutating func free() {
        anychat_free_friend_request_list(&self)
    }
}

extension AnyChatGroupList_C {
    mutating func free() {
        anychat_free_group_list(&self)
    }
}

extension AnyChatGroupMemberList_C {
    mutating func free() {
        anychat_free_group_member_list(&self)
    }
}

extension AnyChatUserList_C {
    mutating func free() {
        anychat_free_user_list(&self)
    }
}

extension AnyChatCallList_C {
    mutating func free() {
        anychat_free_call_list(&self)
    }
}

extension AnyChatMeetingList_C {
    mutating func free() {
        anychat_free_meeting_list(&self)
    }
}

extension AnyChatAuthDeviceList_C {
    mutating func free() {
        anychat_free_auth_device_list(&self)
    }
}
