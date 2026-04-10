//
//  Helpers.swift
//  AnyChatSDK
//
//  Internal utility functions
//

import Foundation

// MARK: - String Conversion

func copyString(_ str: String, to ptr: UnsafeMutablePointer<CChar>, maxLen: Int) {
    let cString = str.utf8CString
    let copyLen = min(cString.count, maxLen)
    ptr.assign(from: cString, count: copyLen)
    if copyLen < maxLen {
        ptr[copyLen] = 0
    }
}

func withCString<R>(_ str: String, _ body: (UnsafePointer<CChar>) -> R) -> R {
    return str.withCString(body)
}

func withOptionalCString<R>(_ str: String?, _ body: (UnsafePointer<CChar>?) -> R) -> R {
    guard let str = str, !str.isEmpty else {
        return body(nil)
    }
    return str.withCString { body($0) }
}

// MARK: - Array Conversion

func withCStringArray<R>(_ strings: [String], _ body: (UnsafeMutablePointer<UnsafePointer<CChar>?>) -> R) -> R {
    var cStrings: [UnsafePointer<CChar>?] = strings.map { strdup($0) }
    defer {
        cStrings.forEach { if let ptr = $0 { free(UnsafeMutableRawPointer(mutating: ptr)) } }
    }
    return cStrings.withUnsafeMutableBufferPointer { buffer in
        body(buffer.baseAddress!)
    }
}

// MARK: - List Conversion

func convertMessageList(_ cList: UnsafePointer<AnyChatMessageList_C>) -> [Message] {
    guard cList.pointee.count > 0, let items = cList.pointee.items else {
        return []
    }

    var messages: [Message] = []
    for i in 0..<Int(cList.pointee.count) {
        messages.append(Message(from: items[i]))
    }
    return messages
}

func convertConversationList(_ cList: UnsafePointer<AnyChatConversationList_C>) -> [Conversation] {
    guard cList.pointee.count > 0, let items = cList.pointee.items else {
        return []
    }

    var conversations: [Conversation] = []
    for i in 0..<Int(cList.pointee.count) {
        conversations.append(Conversation(from: items[i]))
    }
    return conversations
}

func convertFriendList(_ cList: UnsafePointer<AnyChatFriendList_C>) -> [Friend] {
    guard cList.pointee.count > 0, let items = cList.pointee.items else {
        return []
    }

    var friends: [Friend] = []
    for i in 0..<Int(cList.pointee.count) {
        friends.append(Friend(from: items[i]))
    }
    return friends
}

func convertFriendRequestList(_ cList: UnsafePointer<AnyChatFriendRequestList_C>) -> [FriendRequest] {
    guard cList.pointee.count > 0, let items = cList.pointee.items else {
        return []
    }

    var requests: [FriendRequest] = []
    for i in 0..<Int(cList.pointee.count) {
        requests.append(FriendRequest(from: items[i]))
    }
    return requests
}

func convertGroupList(_ cList: UnsafePointer<AnyChatGroupList_C>) -> [Group] {
    guard cList.pointee.count > 0, let items = cList.pointee.items else {
        return []
    }

    var groups: [Group] = []
    for i in 0..<Int(cList.pointee.count) {
        groups.append(Group(from: items[i]))
    }
    return groups
}

func convertGroupMemberList(_ cList: UnsafePointer<AnyChatGroupMemberList_C>) -> [GroupMember] {
    guard cList.pointee.count > 0, let items = cList.pointee.items else {
        return []
    }

    var members: [GroupMember] = []
    for i in 0..<Int(cList.pointee.count) {
        members.append(GroupMember(from: items[i]))
    }
    return members
}

func convertUserList(_ cList: UnsafePointer<AnyChatUserList_C>) -> ([UserInfo], Int64) {
    guard cList.pointee.count > 0, let items = cList.pointee.items else {
        return ([], cList.pointee.total)
    }

    var users: [UserInfo] = []
    for i in 0..<Int(cList.pointee.count) {
        users.append(UserInfo(from: items[i]))
    }
    return (users, cList.pointee.total)
}

func convertCallList(_ cList: UnsafePointer<AnyChatCallList_C>) -> ([CallSession], Int64) {
    guard cList.pointee.count > 0, let items = cList.pointee.items else {
        return ([], cList.pointee.total)
    }

    var calls: [CallSession] = []
    for i in 0..<Int(cList.pointee.count) {
        calls.append(CallSession(from: items[i]))
    }
    return (calls, cList.pointee.total)
}

func convertMeetingList(_ cList: UnsafePointer<AnyChatMeetingList_C>) -> ([MeetingRoom], Int64) {
    guard cList.pointee.count > 0, let items = cList.pointee.items else {
        return ([], cList.pointee.total)
    }

    var meetings: [MeetingRoom] = []
    for i in 0..<Int(cList.pointee.count) {
        meetings.append(MeetingRoom(from: items[i]))
    }
    return (meetings, cList.pointee.total)
}

func convertAuthDeviceList(_ cList: UnsafePointer<AnyChatAuthDeviceList_C>) -> [AuthDevice] {
    guard cList.pointee.count > 0, let items = cList.pointee.items else {
        return []
    }

    var devices: [AuthDevice] = []
    for i in 0..<Int(cList.pointee.count) {
        devices.append(AuthDevice(from: items[i]))
    }
    return devices
}

// MARK: - Error Handling

func getLastError() -> String {
    guard let cStr = anychat_get_last_error() else {
        return ""
    }
    return String(cString: cStr)
}

func checkResult(_ code: Int32) throws {
    guard code == ANYCHAT_OK else {
        let message = getLastError()
        throw AnyChatError(code: Int(code), message: message)
    }
}

// MARK: - Continuation Management

actor ContinuationStore<T: Sendable> {
    private var continuations: [UnsafeMutableRawPointer: CheckedContinuation<T, Error>] = [:]

    func store(_ continuation: CheckedContinuation<T, Error>, for key: UnsafeMutableRawPointer) {
        continuations[key] = continuation
    }

    func resume(for key: UnsafeMutableRawPointer, with result: Result<T, Error>) {
        guard let continuation = continuations.removeValue(forKey: key) else {
            return
        }
        continuation.resume(with: result)
    }
}
