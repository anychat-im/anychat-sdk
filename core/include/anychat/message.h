#pragma once

#include "types.h"

#include <functional>
#include <vector>

namespace anychat {

using MessageCallback = std::function<void(bool success, const std::string& error)>;
using MessageListCallback = std::function<void(const std::vector<Message>& messages, const std::string& error)>;
using OnMessageReceived = std::function<void(const Message& message)>;

class MessageManager {
public:
    virtual ~MessageManager() = default;

    virtual void
    sendTextMessage(const std::string& conv_id, const std::string& content, MessageCallback callback) = 0;

    virtual void
    getHistory(const std::string& conv_id, int64_t before_timestamp, int limit, MessageListCallback callback) = 0;

    virtual void markAsRead(const std::string& conv_id, const std::string& message_id, MessageCallback callback) = 0;

    virtual void setOnMessageReceived(OnMessageReceived handler) = 0;
};

} // namespace anychat
