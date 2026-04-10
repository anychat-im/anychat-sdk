#pragma once

#include "types.h"

#include <functional>
#include <string>
#include <vector>

namespace anychat {

using ConversationListCallback = std::function<void(std::vector<Conversation> list, std::string err)>;
using ConversationCallback = std::function<void(bool ok, std::string err)>;
using OnConversationUpdated = std::function<void(const Conversation& conv)>;

class ConversationManager {
public:
    virtual ~ConversationManager() = default;

    // Returns cached + DB sorted list (pinned first, then by last_msg_time desc)
    virtual void getList(ConversationListCallback cb) = 0;

    // Marks session as read (local + POST /conversations/{id}/read)
    virtual void markRead(const std::string& conv_id, ConversationCallback cb) = 0;

    // Toggle pinned (local + PUT /conversations/{id}/pin)
    virtual void setPinned(const std::string& conv_id, bool pinned, ConversationCallback cb) = 0;

    // Toggle muted (local + PUT /conversations/{id}/mute)
    virtual void setMuted(const std::string& conv_id, bool muted, ConversationCallback cb) = 0;

    // Delete conversation (local + DELETE /conversations/{id})
    virtual void deleteConv(const std::string& conv_id, ConversationCallback cb) = 0;

    // Callback fired whenever a conversation is updated (new message, read, etc.)
    virtual void setOnConversationUpdated(OnConversationUpdated handler) = 0;
};

} // namespace anychat
