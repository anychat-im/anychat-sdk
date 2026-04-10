package com.anychat.sdk

import com.anychat.sdk.models.*

/**
 * Connection state change callback
 */
fun interface ConnectionStateCallback {
    fun onConnectionStateChanged(state: Int)
}

/**
 * Auth result callback
 */
interface AuthCallback {
    fun onAuthResult(success: Boolean, token: AuthToken?, error: String?)
}

/**
 * Generic result callback
 */
fun interface ResultCallback {
    fun onResult(success: Boolean, error: String?)
}

/**
 * Verification code callback
 */
interface VerificationCodeCallback {
    fun onVerificationCodeResult(success: Boolean, result: VerificationCodeResult?, error: String?)
}

/**
 * Auth device list callback
 */
interface AuthDeviceListCallback {
    fun onAuthDeviceList(devices: List<AuthDevice>?, error: String?)
}

/**
 * Message list callback
 */
interface MessageListCallback {
    fun onMessageList(messages: List<Message>?, error: String?)
}

/**
 * Message received callback
 */
fun interface MessageReceivedCallback {
    fun onMessageReceived(message: Message)
}

/**
 * Conversation list callback
 */
interface ConversationListCallback {
    fun onConversationList(conversations: List<Conversation>?, error: String?)
}

/**
 * Conversation updated callback
 */
fun interface ConversationUpdatedCallback {
    fun onConversationUpdated(conversation: Conversation)
}

/**
 * Friend list callback
 */
interface FriendListCallback {
    fun onFriendList(friends: List<Friend>?, error: String?)
}

/**
 * Friend request list callback
 */
interface FriendRequestListCallback {
    fun onFriendRequestList(requests: List<FriendRequest>?, error: String?)
}

/**
 * Friend request received callback
 */
fun interface FriendRequestCallback {
    fun onFriendRequest(request: FriendRequest)
}

/**
 * Group list callback
 */
interface GroupListCallback {
    fun onGroupList(groups: List<Group>?, error: String?)
}

/**
 * Group member list callback
 */
interface GroupMemberListCallback {
    fun onGroupMemberList(members: List<GroupMember>?, error: String?)
}

/**
 * Group invited callback
 */
interface GroupInvitedCallback {
    fun onGroupInvited(group: Group, inviterId: String)
}

/**
 * Group updated callback
 */
fun interface GroupUpdatedCallback {
    fun onGroupUpdated(group: Group)
}
