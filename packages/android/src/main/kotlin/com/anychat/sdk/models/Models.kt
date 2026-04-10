package com.anychat.sdk.models

/**
 * Authentication token
 */
data class AuthToken(
    val accessToken: String,
    val refreshToken: String,
    val expiresAtMs: Long
)

/**
 * Verification code sending result
 */
data class VerificationCodeResult(
    val codeId: String,
    val expiresIn: Long
)

/**
 * Logged-in device info
 */
data class AuthDevice(
    val deviceId: String,
    val deviceType: String,
    val clientVersion: String,
    val lastLoginIp: String,
    val lastLoginAtMs: Long,
    val isCurrent: Boolean
)

/**
 * User basic information
 */
data class UserInfo(
    val userId: String,
    val username: String,
    val avatarUrl: String
)

/**
 * Message types
 */
enum class MessageType(val value: Int) {
    TEXT(0),
    IMAGE(1),
    FILE(2),
    AUDIO(3),
    VIDEO(4)
}

/**
 * Message send states
 */
enum class MessageSendState(val value: Int) {
    PENDING(0),
    SENT(1),
    FAILED(2)
}

/**
 * Message model
 */
data class Message(
    val messageId: String,
    val localId: String,
    val convId: String,
    val senderId: String,
    val contentType: String,
    val type: Int,
    val content: String,
    val seq: Long,
    val replyTo: String,
    val timestampMs: Long,
    val status: Int,
    val sendState: Int,
    val isRead: Boolean
) {
    val messageType: MessageType
        get() = MessageType.values().find { it.value == type } ?: MessageType.TEXT

    val messageSendState: MessageSendState
        get() = MessageSendState.values().find { it.value == sendState } ?: MessageSendState.PENDING
}

/**
 * Conversation types
 */
enum class ConversationType(val value: Int) {
    PRIVATE(0),
    GROUP(1)
}

/**
 * Conversation model
 */
data class Conversation(
    val convId: String,
    val convType: Int,
    val targetId: String,
    val lastMsgId: String,
    val lastMsgText: String,
    val lastMsgTimeMs: Long,
    val unreadCount: Int,
    val isPinned: Boolean,
    val isMuted: Boolean,
    val updatedAtMs: Long
) {
    val conversationType: ConversationType
        get() = ConversationType.values().find { it.value == convType } ?: ConversationType.PRIVATE
}

/**
 * Friend model
 */
data class Friend(
    val userId: String,
    val remark: String,
    val updatedAtMs: Long,
    val isDeleted: Boolean,
    val userInfo: UserInfo
)

/**
 * Friend request model
 */
data class FriendRequest(
    val requestId: Long,
    val fromUserId: String,
    val toUserId: String,
    val message: String,
    val status: String,
    val createdAtMs: Long,
    val fromUserInfo: UserInfo
)

/**
 * Group roles
 */
enum class GroupRole(val value: Int) {
    OWNER(0),
    ADMIN(1),
    MEMBER(2)
}

/**
 * Group model
 */
data class Group(
    val groupId: String,
    val name: String,
    val avatarUrl: String,
    val ownerId: String,
    val memberCount: Int,
    val myRole: Int,
    val joinVerify: Boolean,
    val updatedAtMs: Long
) {
    val groupRole: GroupRole
        get() = GroupRole.values().find { it.value == myRole } ?: GroupRole.MEMBER
}

/**
 * Group member model
 */
data class GroupMember(
    val userId: String,
    val groupNickname: String,
    val role: Int,
    val isMuted: Boolean,
    val joinedAtMs: Long,
    val userInfo: UserInfo
) {
    val memberRole: GroupRole
        get() = GroupRole.values().find { it.value == role } ?: GroupRole.MEMBER
}

/**
 * Connection states
 */
enum class ConnectionState(val value: Int) {
    DISCONNECTED(0),
    CONNECTING(1),
    CONNECTED(2),
    RECONNECTING(3)
}
