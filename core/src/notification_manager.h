#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <shared_mutex>

namespace anychat {

// Payload delivered when the server acknowledges a sent message.
struct MsgSentAck {
    std::string message_id;
    int64_t sequence = 0;
    int64_t timestamp = 0; // Unix seconds
    std::string local_id; // echoed client-generated local ID
};

// Payload delivered for all server-pushed notification events.
struct NotificationEvent {
    std::string notification_type;
    int64_t timestamp = 0; // Unix seconds
    std::string data = "{}";
};

// NotificationManager parses raw JSON frames received from the WebSocket and
// dispatches them to the appropriate registered handler.
//
// Server frame types handled:
//   "pong"          → on_pong_ callback
//   "message.sent"  → on_msg_sent_ callback (MsgSentAck)
//   "notification"  → all registered notification handlers (NotificationEvent)
//
// Unknown type values are silently ignored.
//
// All handler slots are protected by a shared_mutex so they can be replaced
// from any thread without data races.
class NotificationManager {
public:
    using MsgSentHandler = std::function<void(const MsgSentAck&)>;
    using NotifHandler = std::function<void(const NotificationEvent&)>;
    using PongHandler = std::function<void()>;

    NotificationManager() = default;
    ~NotificationManager() = default;

    // Not copyable or movable — handlers are expected to be stable pointers.
    NotificationManager(const NotificationManager&) = delete;
    NotificationManager& operator=(const NotificationManager&) = delete;

    // Register callbacks. Passing nullptr clears the slot (message-sent / pong).
    void setOnMessageSent(MsgSentHandler h);
    void setOnPong(PongHandler h);

    // Append a notification handler.  All registered handlers are invoked on
    // every "notification" frame — each handler is responsible for filtering
    // on event.notification_type.  Multiple subscribers are supported so that
    // independent managers (MessageManager, ConversationManager, etc.) can each
    // register their own handler without overwriting one another.
    void addNotificationHandler(NotifHandler h);

    // Parse `raw_json` and dispatch to the appropriate handler.
    // Called from the WebSocket receive thread. Must not block.
    void handleRaw(const std::string& raw_json);

private:
    mutable std::shared_mutex mu_;

    MsgSentHandler on_msg_sent_;
    std::vector<NotifHandler> notification_handlers_;
    PongHandler on_pong_;
};

} // namespace anychat
