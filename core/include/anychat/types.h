#pragma once

#include <cstdint>
#include <string>

namespace anychat {

enum class ConnectionState
{
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
};

enum class MessageType
{
    Text,
    Image,
    File,
    Audio,
    Video,
};

struct UserInfo {
    std::string user_id;
    std::string username;
    std::string avatar_url;
};

struct Message {
    std::string message_id;
    std::string local_id; // client-generated local ID for dedup
    std::string conv_id; // conversation ID
    std::string sender_id;
    std::string content_type; // "text"|"image"|"audio"|"video"|"file"|"location"|"custom"
    MessageType type = MessageType::Text;
    std::string content; // text or file URL / JSON payload
    int64_t seq = 0; // conversation-scoped sequence number
    std::string reply_to; // message_id being replied to
    int64_t timestamp_ms = 0;
    int status = 0; // 0=normal, 1=recalled, 2=deleted
    int send_state = 0; // 0=pending, 1=sent, 2=failed
    bool is_read = false;
};

// ---- Auth ----------------------------------------------------------------
struct AuthToken {
    std::string access_token;
    std::string refresh_token;
    int64_t expires_at_ms = 0; // Unix ms, 0 = not set
};

struct VerificationCodeResult {
    std::string code_id;
    int64_t expires_in = 0; // seconds
};

struct AuthDevice {
    std::string device_id;
    std::string device_type;
    std::string client_version;
    std::string last_login_ip;
    int64_t last_login_at_ms = 0;
    bool is_current = false;
};

// ---- Conversation --------------------------------------------------------
enum class ConversationType
{
    Private,
    Group
};

struct Conversation {
    std::string conv_id;
    ConversationType conv_type = ConversationType::Private;
    std::string target_id; // user_id for private, group_id for group
    std::string last_msg_id;
    std::string last_msg_text;
    int64_t last_msg_time_ms = 0;
    int32_t unread_count = 0;
    bool is_pinned = false;
    bool is_muted = false;
    int64_t pin_time_ms = 0; // used for sort order
    int64_t local_seq = 0;
    int64_t updated_at_ms = 0;
};

// ---- Friend --------------------------------------------------------------
struct Friend {
    std::string user_id;
    std::string remark;
    int64_t updated_at_ms = 0;
    bool is_deleted = false;
    UserInfo user_info;
};

struct FriendRequest {
    int64_t request_id = 0;
    std::string from_user_id;
    std::string to_user_id;
    std::string message;
    std::string status; // "pending" | "accepted" | "rejected"
    int64_t created_at_ms = 0;
    UserInfo from_user_info;
};

// ---- Group ---------------------------------------------------------------
enum class GroupRole
{
    Owner,
    Admin,
    Member
};

struct Group {
    std::string group_id;
    std::string name;
    std::string avatar_url;
    std::string owner_id;
    int32_t member_count = 0;
    GroupRole my_role = GroupRole::Member;
    bool join_verify = false;
    int64_t updated_at_ms = 0;
};

struct GroupMember {
    std::string user_id;
    std::string group_nickname;
    GroupRole role = GroupRole::Member;
    bool is_muted = false;
    int64_t joined_at_ms = 0;
    UserInfo user_info;
};

// ---- File ----------------------------------------------------------------
struct FileInfo {
    std::string file_id;
    std::string file_name;
    std::string file_type; // "image" | "video" | "audio" | "file"
    int64_t file_size_bytes = 0;
    std::string mime_type;
    std::string download_url;
    int64_t created_at_ms = 0;
};

// ---- Outbound message state ----------------------------------------------
enum class SendState
{
    Received = 0,
    Sending = 1,
    Failed = 2
};

// ---- User ----------------------------------------------------------------
struct UserProfile {
    std::string user_id;
    std::string nickname;
    std::string avatar_url;
    std::string phone;
    std::string email;
    std::string signature;
    std::string region;
    int32_t gender = 0; // 0=unknown, 1=male, 2=female
    int64_t created_at_ms = 0;
};

struct UserSettings {
    bool notification_enabled = true;
    bool sound_enabled = true;
    bool vibration_enabled = true;
    bool message_preview_enabled = true;
    bool friend_verify_required = false;
    bool search_by_phone = true;
    bool search_by_id = true;
    std::string language;
};

// ---- Call -----------------------------------------------------------------
enum class CallType
{
    Audio,
    Video
};
enum class CallStatus
{
    Ringing,
    Connected,
    Ended,
    Rejected,
    Missed,
    Cancelled
};

struct CallSession {
    std::string call_id;
    std::string caller_id;
    std::string callee_id;
    CallType call_type = CallType::Audio;
    CallStatus status = CallStatus::Ringing;
    std::string room_name;
    std::string token; // Call JWT — filled on initiateCall / joinCall
    int64_t started_at = 0; // Unix seconds
    int64_t connected_at = 0;
    int64_t ended_at = 0;
    int32_t duration = 0; // seconds
};

struct MeetingRoom {
    std::string room_id;
    std::string creator_id;
    std::string title;
    std::string room_name;
    std::string token; // Call JWT — filled on createMeeting / joinMeeting
    bool has_password = false;
    int32_t max_participants = 0;
    bool is_active = true;
    int64_t started_at = 0; // Unix seconds
    int64_t created_at_ms = 0;
};

} // namespace anychat
