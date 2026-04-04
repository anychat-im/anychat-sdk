//
//  Models.swift
//  AnyChatSDK
//
//  Swift model types wrapping C structures
//

import Foundation

// MARK: - Connection State

public enum ConnectionState: Int, Sendable {
    case disconnected = 0
    case connecting = 1
    case connected = 2
    case reconnecting = 3
}

// MARK: - Error

public enum AnyChatError: Error, Sendable {
    case invalidParam
    case auth
    case network
    case timeout
    case notFound
    case alreadyExists
    case `internal`
    case notLoggedIn
    case tokenExpired
    case unknown(code: Int, message: String)

    init(code: Int, message: String = "") {
        switch code {
        case 0: self = .unknown(code: 0, message: "Success")
        case 1: self = .invalidParam
        case 2: self = .auth
        case 3: self = .network
        case 4: self = .timeout
        case 5: self = .notFound
        case 6: self = .alreadyExists
        case 7: self = .`internal`
        case 8: self = .notLoggedIn
        case 9: self = .tokenExpired
        default: self = .unknown(code: code, message: message)
        }
    }
}

// MARK: - Auth Token

public struct AuthToken: Sendable {
    public let accessToken: String
    public let refreshToken: String
    public let expiresAt: Date

    public init(accessToken: String, refreshToken: String, expiresAt: Date) {
        self.accessToken = accessToken
        self.refreshToken = refreshToken
        self.expiresAt = expiresAt
    }

    init(from cToken: AnyChatAuthToken_C) {
        self.accessToken = String(cString: &cToken.access_token.0)
        self.refreshToken = String(cString: &cToken.refresh_token.0)
        self.expiresAt = Date(timeIntervalSince1970: Double(cToken.expires_at_ms) / 1000.0)
    }

    func toCStruct() -> AnyChatAuthToken_C {
        var token = AnyChatAuthToken_C()
        withUnsafeMutableBytes(of: &token.access_token) { ptr in
            _ = accessToken.utf8CString.withUnsafeBytes { src in
                ptr.copyBytes(from: src.prefix(511))
            }
        }
        withUnsafeMutableBytes(of: &token.refresh_token) { ptr in
            _ = refreshToken.utf8CString.withUnsafeBytes { src in
                ptr.copyBytes(from: src.prefix(511))
            }
        }
        token.expires_at_ms = Int64(expiresAt.timeIntervalSince1970 * 1000)
        return token
    }
}

// MARK: - User Info

public struct UserInfo: Sendable {
    public let userId: String
    public let username: String
    public let avatarURL: String

    public init(userId: String, username: String, avatarURL: String) {
        self.userId = userId
        self.username = username
        self.avatarURL = avatarURL
    }

    init(from cInfo: AnyChatUserInfo_C) {
        self.userId = String(cString: &cInfo.user_id.0)
        self.username = String(cString: &cInfo.username.0)
        self.avatarURL = String(cString: &cInfo.avatar_url.0)
    }
}

// MARK: - Message

public enum MessageType: Int, Sendable {
    case text = 0
    case image = 1
    case file = 2
    case audio = 3
    case video = 4
}

public enum MessageSendState: Int, Sendable {
    case pending = 0
    case sent = 1
    case failed = 2
}

public struct Message: Sendable {
    public let messageId: String
    public let localId: String
    public let conversationId: String
    public let senderId: String
    public let contentType: String
    public let type: MessageType
    public let content: String
    public let seq: Int64
    public let replyTo: String
    public let timestamp: Date
    public let status: Int
    public let sendState: MessageSendState
    public let isRead: Bool

    init(from cMsg: AnyChatMessage_C) {
        self.messageId = String(cString: &cMsg.message_id.0)
        self.localId = String(cString: &cMsg.local_id.0)
        self.conversationId = String(cString: &cMsg.conv_id.0)
        self.senderId = String(cString: &cMsg.sender_id.0)
        self.contentType = String(cString: &cMsg.content_type.0)
        self.type = MessageType(rawValue: Int(cMsg.type)) ?? .text
        self.content = cMsg.content != nil ? String(cString: cMsg.content) : ""
        self.seq = cMsg.seq
        self.replyTo = String(cString: &cMsg.reply_to.0)
        self.timestamp = Date(timeIntervalSince1970: Double(cMsg.timestamp_ms) / 1000.0)
        self.status = Int(cMsg.status)
        self.sendState = MessageSendState(rawValue: Int(cMsg.send_state)) ?? .pending
        self.isRead = cMsg.is_read != 0
    }
}

// MARK: - Conversation

public enum ConversationType: Int, Sendable {
    case `private` = 0
    case group = 1
}

public struct Conversation: Sendable {
    public let conversationId: String
    public let type: ConversationType
    public let targetId: String
    public let lastMessageId: String
    public let lastMessageText: String
    public let lastMessageTime: Date
    public let unreadCount: Int
    public let isPinned: Bool
    public let isMuted: Bool
    public let updatedAt: Date

    init(from cConv: AnyChatConversation_C) {
        self.conversationId = String(cString: &cConv.conv_id.0)
        self.type = ConversationType(rawValue: Int(cConv.conv_type)) ?? .private
        self.targetId = String(cString: &cConv.target_id.0)
        self.lastMessageId = String(cString: &cConv.last_msg_id.0)
        self.lastMessageText = String(cString: &cConv.last_msg_text.0)
        self.lastMessageTime = Date(timeIntervalSince1970: Double(cConv.last_msg_time_ms) / 1000.0)
        self.unreadCount = Int(cConv.unread_count)
        self.isPinned = cConv.is_pinned != 0
        self.isMuted = cConv.is_muted != 0
        self.updatedAt = Date(timeIntervalSince1970: Double(cConv.updated_at_ms) / 1000.0)
    }
}

// MARK: - Friend

public struct Friend: Sendable {
    public let userId: String
    public let remark: String
    public let updatedAt: Date
    public let isDeleted: Bool
    public let userInfo: UserInfo

    init(from cFriend: AnyChatFriend_C) {
        self.userId = String(cString: &cFriend.user_id.0)
        self.remark = String(cString: &cFriend.remark.0)
        self.updatedAt = Date(timeIntervalSince1970: Double(cFriend.updated_at_ms) / 1000.0)
        self.isDeleted = cFriend.is_deleted != 0
        self.userInfo = UserInfo(from: cFriend.user_info)
    }
}

public struct FriendRequest: Sendable {
    public let requestId: Int64
    public let fromUserId: String
    public let toUserId: String
    public let message: String
    public let status: String
    public let createdAt: Date
    public let fromUserInfo: UserInfo

    init(from cReq: AnyChatFriendRequest_C) {
        self.requestId = cReq.request_id
        self.fromUserId = String(cString: &cReq.from_user_id.0)
        self.toUserId = String(cString: &cReq.to_user_id.0)
        self.message = String(cString: &cReq.message.0)
        self.status = String(cString: &cReq.status.0)
        self.createdAt = Date(timeIntervalSince1970: Double(cReq.created_at_ms) / 1000.0)
        self.fromUserInfo = UserInfo(from: cReq.from_user_info)
    }
}

// MARK: - Group

public enum GroupRole: Int, Sendable {
    case owner = 0
    case admin = 1
    case member = 2
}

public struct Group: Sendable {
    public let groupId: String
    public let name: String
    public let avatarURL: String
    public let ownerId: String
    public let memberCount: Int
    public let myRole: GroupRole
    public let joinVerify: Bool
    public let updatedAt: Date

    init(from cGroup: AnyChatGroup_C) {
        self.groupId = String(cString: &cGroup.group_id.0)
        self.name = String(cString: &cGroup.name.0)
        self.avatarURL = String(cString: &cGroup.avatar_url.0)
        self.ownerId = String(cString: &cGroup.owner_id.0)
        self.memberCount = Int(cGroup.member_count)
        self.myRole = GroupRole(rawValue: Int(cGroup.my_role)) ?? .member
        self.joinVerify = cGroup.join_verify != 0
        self.updatedAt = Date(timeIntervalSince1970: Double(cGroup.updated_at_ms) / 1000.0)
    }
}

public struct GroupMember: Sendable {
    public let userId: String
    public let groupNickname: String
    public let role: GroupRole
    public let isMuted: Bool
    public let joinedAt: Date
    public let userInfo: UserInfo

    init(from cMember: AnyChatGroupMember_C) {
        self.userId = String(cString: &cMember.user_id.0)
        self.groupNickname = String(cString: &cMember.group_nickname.0)
        self.role = GroupRole(rawValue: Int(cMember.role)) ?? .member
        self.isMuted = cMember.is_muted != 0
        self.joinedAt = Date(timeIntervalSince1970: Double(cMember.joined_at_ms) / 1000.0)
        self.userInfo = UserInfo(from: cMember.user_info)
    }
}

// MARK: - File

public struct FileInfo: Sendable {
    public let fileId: String
    public let fileName: String
    public let fileType: String
    public let fileSize: Int64
    public let mimeType: String
    public let downloadURL: String
    public let createdAt: Date

    init(from cFile: AnyChatFileInfo_C) {
        self.fileId = String(cString: &cFile.file_id.0)
        self.fileName = String(cString: &cFile.file_name.0)
        self.fileType = String(cString: &cFile.file_type.0)
        self.fileSize = cFile.file_size_bytes
        self.mimeType = String(cString: &cFile.mime_type.0)
        self.downloadURL = String(cString: &cFile.download_url.0)
        self.createdAt = Date(timeIntervalSince1970: Double(cFile.created_at_ms) / 1000.0)
    }
}

// MARK: - User Profile

public struct UserProfile: Sendable {
    public let userId: String
    public let nickname: String
    public let avatarURL: String
    public let phone: String
    public let email: String
    public let signature: String
    public let region: String
    public let gender: Int
    public let createdAt: Date

    init(from cProfile: AnyChatUserProfile_C) {
        self.userId = String(cString: &cProfile.user_id.0)
        self.nickname = String(cString: &cProfile.nickname.0)
        self.avatarURL = String(cString: &cProfile.avatar_url.0)
        self.phone = String(cString: &cProfile.phone.0)
        self.email = String(cString: &cProfile.email.0)
        self.signature = String(cString: &cProfile.signature.0)
        self.region = String(cString: &cProfile.region.0)
        self.gender = Int(cProfile.gender)
        self.createdAt = Date(timeIntervalSince1970: Double(cProfile.created_at_ms) / 1000.0)
    }

    func toCStruct() -> AnyChatUserProfile_C {
        var profile = AnyChatUserProfile_C()
        copyString(userId, to: &profile.user_id.0, maxLen: 64)
        copyString(nickname, to: &profile.nickname.0, maxLen: 128)
        copyString(avatarURL, to: &profile.avatar_url.0, maxLen: 512)
        copyString(phone, to: &profile.phone.0, maxLen: 32)
        copyString(email, to: &profile.email.0, maxLen: 128)
        copyString(signature, to: &profile.signature.0, maxLen: 256)
        copyString(region, to: &profile.region.0, maxLen: 64)
        profile.gender = Int32(gender)
        profile.created_at_ms = Int64(createdAt.timeIntervalSince1970 * 1000)
        return profile
    }
}

public struct UserSettings: Sendable {
    public let notificationEnabled: Bool
    public let soundEnabled: Bool
    public let vibrationEnabled: Bool
    public let messagePreviewEnabled: Bool
    public let friendVerifyRequired: Bool
    public let searchByPhone: Bool
    public let searchById: Bool
    public let language: String

    init(from cSettings: AnyChatUserSettings_C) {
        self.notificationEnabled = cSettings.notification_enabled != 0
        self.soundEnabled = cSettings.sound_enabled != 0
        self.vibrationEnabled = cSettings.vibration_enabled != 0
        self.messagePreviewEnabled = cSettings.message_preview_enabled != 0
        self.friendVerifyRequired = cSettings.friend_verify_required != 0
        self.searchByPhone = cSettings.search_by_phone != 0
        self.searchById = cSettings.search_by_id != 0
        self.language = String(cString: &cSettings.language.0)
    }

    func toCStruct() -> AnyChatUserSettings_C {
        var settings = AnyChatUserSettings_C()
        settings.notification_enabled = notificationEnabled ? 1 : 0
        settings.sound_enabled = soundEnabled ? 1 : 0
        settings.vibration_enabled = vibrationEnabled ? 1 : 0
        settings.message_preview_enabled = messagePreviewEnabled ? 1 : 0
        settings.friend_verify_required = friendVerifyRequired ? 1 : 0
        settings.search_by_phone = searchByPhone ? 1 : 0
        settings.search_by_id = searchById ? 1 : 0
        copyString(language, to: &settings.language.0, maxLen: 16)
        return settings
    }
}

// MARK: - Call

public enum CallType: Int, Sendable {
    case audio = 0
    case video = 1
}

public enum CallStatus: Int, Sendable {
    case ringing = 0
    case connected = 1
    case ended = 2
    case rejected = 3
    case missed = 4
    case cancelled = 5
}

public struct CallSession: Sendable {
    public let callId: String
    public let callerId: String
    public let calleeId: String
    public let callType: CallType
    public let status: CallStatus
    public let roomName: String
    public let token: String
    public let startedAt: Date
    public let connectedAt: Date
    public let endedAt: Date
    public let duration: Int

    init(from cSession: AnyChatCallSession_C) {
        self.callId = String(cString: &cSession.call_id.0)
        self.callerId = String(cString: &cSession.caller_id.0)
        self.calleeId = String(cString: &cSession.callee_id.0)
        self.callType = CallType(rawValue: Int(cSession.call_type)) ?? .audio
        self.status = CallStatus(rawValue: Int(cSession.status)) ?? .ringing
        self.roomName = String(cString: &cSession.room_name.0)
        self.token = String(cString: &cSession.token.0)
        self.startedAt = Date(timeIntervalSince1970: Double(cSession.started_at))
        self.connectedAt = Date(timeIntervalSince1970: Double(cSession.connected_at))
        self.endedAt = Date(timeIntervalSince1970: Double(cSession.ended_at))
        self.duration = Int(cSession.duration)
    }
}

public struct MeetingRoom: Sendable {
    public let roomId: String
    public let creatorId: String
    public let title: String
    public let roomName: String
    public let token: String
    public let hasPassword: Bool
    public let maxParticipants: Int
    public let isActive: Bool
    public let startedAt: Date
    public let createdAt: Date

    init(from cRoom: AnyChatMeetingRoom_C) {
        self.roomId = String(cString: &cRoom.room_id.0)
        self.creatorId = String(cString: &cRoom.creator_id.0)
        self.title = String(cString: &cRoom.title.0)
        self.roomName = String(cString: &cRoom.room_name.0)
        self.token = String(cString: &cRoom.token.0)
        self.hasPassword = cRoom.has_password != 0
        self.maxParticipants = Int(cRoom.max_participants)
        self.isActive = cRoom.is_active != 0
        self.startedAt = Date(timeIntervalSince1970: Double(cRoom.started_at))
        self.createdAt = Date(timeIntervalSince1970: Double(cRoom.created_at_ms) / 1000.0)
    }
}

// MARK: - Client Config

public struct ClientConfig {
    public let gatewayURL: String
    public let apiBaseURL: String
    public let deviceId: String
    public let dbPath: String
    public let connectTimeoutMs: Int
    public let maxReconnectAttempts: Int
    public let autoReconnect: Bool

    public init(
        gatewayURL: String,
        apiBaseURL: String,
        deviceId: String,
        dbPath: String,
        connectTimeoutMs: Int = 10000,
        maxReconnectAttempts: Int = 5,
        autoReconnect: Bool = true
    ) {
        self.gatewayURL = gatewayURL
        self.apiBaseURL = apiBaseURL
        self.deviceId = deviceId
        self.dbPath = dbPath
        self.connectTimeoutMs = connectTimeoutMs
        self.maxReconnectAttempts = maxReconnectAttempts
        self.autoReconnect = autoReconnect
    }
}
