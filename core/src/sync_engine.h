#pragma once

#include "cache/conversation_cache.h"
#include "cache/message_cache.h"
#include "db/database.h"
#include "network/http_client.h"

#include <memory>
#include <string>

namespace anychat {

// SyncEngine performs incremental data sync against the POST /sync endpoint.
//
// Called by ConnectionManager (via the on_ready hook) each time the WebSocket
// connection is established.  It reads the persisted last_sync_time from the
// local database, collects the current per-conversation sequence numbers,
// posts the sync request, and merges the response into both the database and
// the in-memory caches.
class SyncEngine {
public:
    SyncEngine(
        db::Database* db,
        cache::ConversationCache* conv_cache,
        cache::MessageCache* msg_cache,
        std::shared_ptr<network::HttpClient> http
    );

    // Trigger an incremental sync.  Reads last_sync_time from the database,
    // builds the sync request, and posts it to /sync.  The response is merged
    // into the database and caches asynchronously on the HTTP worker thread.
    void sync();

private:
    // Parse and dispatch the raw JSON response body.
    void handleSyncResponse(const std::string& body);

    db::Database* db_;
    cache::ConversationCache* conv_cache_;
    cache::MessageCache* msg_cache_;
    std::shared_ptr<network::HttpClient> http_;
};

} // namespace anychat
