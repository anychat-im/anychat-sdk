#include "notification_manager.h"
#include "json_common.h"

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <utility>
#include <variant>

namespace anychat {
namespace {

using json_common::readJsonRelaxed;
using json_common::parseInt64Value;

using IntegerValue = std::variant<int64_t, double, std::string>;
using OptionalIntegerValue = std::optional<IntegerValue>;

struct WsFramePayload {
    std::string type{};
    std::optional<glz::raw_json> payload{};
};

struct MsgSentAckPayload {
    std::string message_id{};
    OptionalIntegerValue sequence{};
    OptionalIntegerValue timestamp{};
    std::string local_id{};
};

struct NotificationEnvelopePayload {
    std::string type{};
    OptionalIntegerValue timestamp{};
    std::optional<glz::raw_json> payload{};
};

template <typename T>
bool parseRawObject(const std::optional<glz::raw_json>& raw, T& out) {
    if (!raw.has_value() || raw->str.empty()) {
        return false;
    }

    std::string err;
    return readJsonRelaxed(std::string(raw->str), out, err);
}

std::string rawJsonToString(const std::optional<glz::raw_json>& raw) {
    if (!raw.has_value() || raw->str.empty()) {
        return "";
    }
    return std::string(raw->str);
}

} // namespace

// ---------------------------------------------------------------------------
// Handler registration
// ---------------------------------------------------------------------------

void NotificationManager::setOnMessageSent(MsgSentHandler h) {
    std::unique_lock lock(mu_);
    on_msg_sent_ = std::move(h);
}

void NotificationManager::addNotificationHandler(NotifHandler h) {
    std::unique_lock lock(mu_);
    notification_handlers_.push_back(std::move(h));
}

void NotificationManager::setOnPong(PongHandler h) {
    std::unique_lock lock(mu_);
    on_pong_ = std::move(h);
}

// ---------------------------------------------------------------------------
// Frame dispatch
// ---------------------------------------------------------------------------

void NotificationManager::handleRaw(const std::string& raw_json) {
    WsFramePayload frame{};
    std::string err;
    if (!readJsonRelaxed(raw_json, frame, err)) {
        return;
    }
    const std::string& type = frame.type;
    if (type.empty()) {
        return;
    }

    // ---- pong ---------------------------------------------------------------
    if (type == "pong") {
        PongHandler handler;
        {
            std::shared_lock lock(mu_);
            handler = on_pong_;
        }
        if (handler) {
            handler();
        }
        return;
    }

    // ---- message.sent -------------------------------------------------------
    if (type == "message.sent") {
        MsgSentAckPayload payload{};
        if (!parseRawObject(frame.payload, payload)) {
            return;
        }

        MsgSentAck ack;
        ack.message_id = payload.message_id;
        ack.sequence = parseInt64Value(payload.sequence, 0);
        ack.timestamp = parseInt64Value(payload.timestamp, 0);
        ack.local_id = payload.local_id;

        MsgSentHandler handler;
        {
            std::shared_lock lock(mu_);
            handler = on_msg_sent_;
        }
        if (handler) {
            handler(ack);
        }
        return;
    }

    // ---- notification -------------------------------------------------------
    if (type == "notification") {
        NotificationEnvelopePayload payload{};
        if (!parseRawObject(frame.payload, payload)) {
            return;
        }

        NotificationEvent evt;
        evt.notification_type = payload.type;
        if (evt.notification_type.empty()) {
            return;
        }
        evt.timestamp = parseInt64Value(payload.timestamp, 0);
        evt.data = rawJsonToString(payload.payload);
        if (evt.data.empty()) {
            evt.data = "{}";
        }

        std::vector<NotifHandler> handlers;
        {
            std::shared_lock lock(mu_);
            handlers = notification_handlers_;
        }
        for (const auto& h : handlers) {
            if (h) {
                h(evt);
            }
        }
        return;
    }

    // Unknown type — silently ignore.
}

} // namespace anychat
