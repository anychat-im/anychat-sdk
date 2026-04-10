#include "utils_c.h"

#include "anychat_c/types_c.h"

#include <cstdlib>
#include <cstring>

/* ---- Internal helpers (declared in utils_c.h) ---- */

char* anychat_strdup(const char* s) {
    if (!s)
        return nullptr;
    size_t len = std::strlen(s) + 1;
    char* copy = static_cast<char*>(std::malloc(len));
    if (copy)
        std::memcpy(copy, s, len);
    return copy;
}

/* Safely copy at most (dst_size - 1) bytes and always NUL-terminate. */
void anychat_strlcpy(char* dst, const char* src, size_t dst_size) {
    if (!dst || dst_size == 0)
        return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t i = 0;
    for (; i < dst_size - 1 && src[i] != '\0'; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}

extern "C" {

void anychat_free_string(char* str) {
    std::free(str);
}

void anychat_free_message(AnyChatMessage_C* msg) {
    if (!msg)
        return;
    std::free(msg->content);
    msg->content = nullptr;
}

void anychat_free_message_list(AnyChatMessageList_C* list) {
    if (!list)
        return;
    for (int i = 0; i < list->count; ++i)
        std::free(list->items[i].content);
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_conversation_list(AnyChatConversationList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_friend_list(AnyChatFriendList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_friend_request_list(AnyChatFriendRequestList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_blacklist_list(AnyChatBlacklistList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_group_list(AnyChatGroupList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_group_member_list(AnyChatGroupMemberList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_group_join_request_list(AnyChatGroupJoinRequestList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_user_list(AnyChatUserList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_call_list(AnyChatCallList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_meeting_list(AnyChatMeetingList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_auth_device_list(AnyChatAuthDeviceList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
}

void anychat_free_file_list(AnyChatFileList_C* list) {
    if (!list)
        return;
    std::free(list->items);
    list->items = nullptr;
    list->count = 0;
    list->total = 0;
    list->page = 0;
    list->page_size = 0;
}

} // extern "C"
