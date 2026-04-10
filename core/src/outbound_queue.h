#pragma once

#include "notification_manager.h"

#include "../include/anychat/message.h"
#include "db/database.h"

#include <functional>
// `memory` include not needed (no shared_ptr)
#include <mutex>
#include <string>
#include <unordered_map>

namespace anychat {

// OutboundQueue provides reliable, SQLite-backed message delivery.
//
// Lifecycle:
//   1. Call enqueue() to persist a message and store its completion callback.
//   2. Call onConnected(send_fn) whenever the WebSocket connects; the queue
//      will immediately flush all rows whose status is "pending".
//   3. Call onMessageSentAck() with acks received via NotificationManager to
//      mark rows as sent and invoke their callbacks.
//   4. Call onDisconnected() when the WebSocket drops; the queue stops
//      sending but retains pending rows for the next onConnected() call.
//
// Thread-safety: all public methods are safe to call from any thread.
class OutboundQueue {
public:
    // `send_fn` is provided by onConnected(); it wraps the live WebSocket send.
    using SendFn = std::function<void(const std::string& json_payload)>;

    explicit OutboundQueue(db::Database* db);
    ~OutboundQueue() = default;

    OutboundQueue(const OutboundQueue&) = delete;
    OutboundQueue& operator=(const OutboundQueue&) = delete;

    // Persist a new outbound message and register its callback.
    // `conv_type` defaults to "private".  Pass "group" for group conversations.
    // `local_id`  must be globally unique (caller-generated UUID or similar).
    void enqueue(
        const std::string& conv_id,
        const std::string& conv_type,
        const std::string& content_type,
        const std::string& content,
        const std::string& local_id,
        MessageCallback cb
    );

    // Called when the WebSocket connection is established.
    // Flushes all pending rows through `send_fn`.
    void onConnected(SendFn send_fn);

    // Called when the WebSocket connection is lost.
    // Clears the active send function; pending rows will be retried next time
    // onConnected() is called.
    void onDisconnected();

    // Send a transient frame without persisting/retrying. Returns false when
    // no active WebSocket sender is available.
    bool sendTransient(const std::string& json_payload);

    // Called by NotificationManager (or equivalent) when the server echoes a
    // message.sent acknowledgement.  Matches by local_id, marks the DB row as
    // sent (status=1), and invokes the stored callback with success=true.
    void onMessageSentAck(const MsgSentAck& ack);

private:
    // Build the JSON payload for a message.send frame.
    static std::string buildSendFrame(
        const std::string& conv_id,
        const std::string& conv_type,
        const std::string& content_type,
        const std::string& content,
        const std::string& local_id
    );

    // Send a single row and increment its retry_count in the DB.
    // Must be called while holding mu_ (shared or exclusive as appropriate)
    // and while send_fn_ is valid.
    void sendRow(
        const std::string& conv_id,
        const std::string& conv_type,
        const std::string& content_type,
        const std::string& content,
        const std::string& local_id
    );

    db::Database* db_;

    std::mutex mu_;

    // Active WebSocket send function; null when disconnected.
    SendFn send_fn_;

    // In-memory map from local_id → completion callback.
    // Populated by enqueue(), consumed by onMessageSentAck().
    std::unordered_map<std::string, MessageCallback> callbacks_;
};

} // namespace anychat
