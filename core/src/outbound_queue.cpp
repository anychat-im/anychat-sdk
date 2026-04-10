#include "outbound_queue.h"

#include <chrono>
#include <utility>

#include <nlohmann/json.hpp>

namespace anychat {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

OutboundQueue::OutboundQueue(db::Database* db)
    : db_(db) {}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void OutboundQueue::enqueue(
    const std::string& conv_id,
    const std::string& conv_type,
    const std::string& content_type,
    const std::string& content,
    const std::string& local_id,
    MessageCallback cb
) {
    const int64_t now_s =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // Persist the row.  The DB schema (v1) has:
    //   local_id, conv_id, conv_type, content_type, content,
    //   retry_count DEFAULT 0, created_at
    // Use INSERT OR IGNORE so a duplicate local_id is a no-op (idempotent).
    db_->exec(
        "INSERT OR IGNORE INTO outbound_queue "
        "(local_id, conv_id, conv_type, content_type, content, retry_count, created_at) "
        "VALUES (?, ?, ?, ?, ?, 0, ?)",
        { local_id, conv_id, conv_type, content_type, content, now_s }
    );

    // Store the callback in memory and, if connected, send immediately.
    SendFn current_send_fn;
    {
        std::lock_guard lock(mu_);
        if (cb) {
            callbacks_.emplace(local_id, std::move(cb));
        }
        current_send_fn = send_fn_;
    }

    if (current_send_fn) {
        sendRow(conv_id, conv_type, content_type, content, local_id);
    }
}

void OutboundQueue::onConnected(SendFn send_fn) {
    {
        std::lock_guard lock(mu_);
        send_fn_ = std::move(send_fn);
    }

    // Flush all pending rows from the DB (retry_count is informational only;
    // we re-send every pending row on reconnect).
    db::Rows rows = db_->querySync("SELECT local_id, conv_id, conv_type, content_type, content "
                                   "FROM outbound_queue ORDER BY created_at ASC");

    for (const auto& row : rows) {
        // Guard: skip rows with missing mandatory columns.
        auto it_lid = row.find("local_id");
        auto it_cid = row.find("conv_id");
        auto it_ct = row.find("conv_type");
        auto it_ctype = row.find("content_type");
        auto it_body = row.find("content");

        if (it_lid == row.end() || it_cid == row.end() || it_ct == row.end() || it_ctype == row.end()
            || it_body == row.end()) {
            continue;
        }

        sendRow(it_cid->second, it_ct->second, it_ctype->second, it_body->second, it_lid->second);
    }
}

void OutboundQueue::onDisconnected() {
    std::lock_guard lock(mu_);
    send_fn_ = nullptr;
}

bool OutboundQueue::sendTransient(const std::string& json_payload) {
    SendFn fn;
    {
        std::lock_guard lock(mu_);
        fn = send_fn_;
    }
    if (!fn) {
        return false;
    }
    fn(json_payload);
    return true;
}

void OutboundQueue::onMessageSentAck(const MsgSentAck& ack) {
    if (ack.local_id.empty()) {
        return;
    }

    // Remove the row from the DB.
    db_->exec("DELETE FROM outbound_queue WHERE local_id = ?", { ack.local_id });

    // Extract and invoke the callback outside the lock.
    MessageCallback cb;
    {
        std::lock_guard lock(mu_);
        auto it = callbacks_.find(ack.local_id);
        if (it != callbacks_.end()) {
            cb = std::move(it->second);
            callbacks_.erase(it);
        }
    }

    if (cb) {
        cb(true, "");
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

// static
std::string OutboundQueue::buildSendFrame(
    const std::string& conv_id,
    const std::string& conv_type,
    const std::string& content_type,
    const std::string& content,
    const std::string& local_id
) {
    nlohmann::json frame = { { "type", "message.send" },
                             { "payload",
                               { { "conversationId", conv_id },
                                 { "conversationType", conv_type },
                                 { "contentType", content_type },
                                 { "content", content },
                                 { "localId", local_id } } } };
    return frame.dump();
}

void OutboundQueue::sendRow(
    const std::string& conv_id,
    const std::string& conv_type,
    const std::string& content_type,
    const std::string& content,
    const std::string& local_id
) {
    // Capture send_fn under the lock, then call it outside.
    SendFn fn;
    {
        std::lock_guard lock(mu_);
        fn = send_fn_;
    }
    if (!fn) {
        return;
    }

    const std::string payload = buildSendFrame(conv_id, conv_type, content_type, content, local_id);

    // Bump retry_count in the DB before sending (best-effort; ignore errors).
    db_->exec("UPDATE outbound_queue SET retry_count = retry_count + 1 WHERE local_id = ?", { local_id });

    fn(payload);
}

} // namespace anychat
