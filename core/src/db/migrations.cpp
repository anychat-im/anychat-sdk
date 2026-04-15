#include "migrations.h"

#include <cstdio>

#include <sqlite3.h>

namespace anychat::db {

namespace {

// Run a raw SQL string directly on `db` (no parameter binding needed here).
// Returns true on SQLITE_OK / SQLITE_DONE / SQLITE_ROW, false otherwise.
static bool execRaw(sqlite3* db, const char* sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg) {
            std::fprintf(stderr, "[anychat::db] migration error: %s\n", errmsg);
            sqlite3_free(errmsg);
        }
        return false;
    }
    return true;
}

// Read PRAGMA user_version.
static int getUserVersion(sqlite3* db) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }
    int ver = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ver = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return ver;
}

// Full DDL for schema version 1.
static constexpr const char* kSchemav1 = R"sql(
CREATE TABLE IF NOT EXISTS users (
    user_id       TEXT PRIMARY KEY,
    nickname      TEXT,
    avatar_url    TEXT,
    signature     TEXT,
    updated_at_ms INTEGER
);

CREATE TABLE IF NOT EXISTS friends (
    user_id         TEXT PRIMARY KEY,
    remark          TEXT,
    updated_at_ms   INTEGER,
    is_deleted      INTEGER DEFAULT 0,
    friend_nickname TEXT,
    friend_avatar   TEXT
);

CREATE TABLE IF NOT EXISTS conversations (
    conv_id               TEXT PRIMARY KEY,
    conv_type             TEXT,
    target_id             TEXT,
    last_msg_id           TEXT,
    last_msg_text         TEXT,
    last_msg_time_ms      INTEGER,
    unread_count          INTEGER DEFAULT 0,
    is_pinned             INTEGER DEFAULT 0,
    is_muted              INTEGER DEFAULT 0,
    burn_after_reading    INTEGER DEFAULT 0,
    auto_delete_duration  INTEGER DEFAULT 0,
    pin_time_ms           INTEGER DEFAULT 0,
    local_seq             INTEGER DEFAULT 0,
    updated_at_ms         INTEGER
);

CREATE TABLE IF NOT EXISTS messages (
    message_id   TEXT PRIMARY KEY,
    local_id     TEXT UNIQUE,
    conv_id      TEXT NOT NULL,
    sender_id    TEXT,
    content_type TEXT,
    content      TEXT,
    seq          INTEGER,
    reply_to     TEXT,
    status       INTEGER DEFAULT 0,
    send_state   INTEGER DEFAULT 0,
    is_read      INTEGER DEFAULT 0,
    timestamp_ms INTEGER,
    FOREIGN KEY (conv_id) REFERENCES conversations(conv_id)
);

CREATE INDEX IF NOT EXISTS idx_messages_conv_seq
    ON messages (conv_id, seq DESC);

CREATE TABLE IF NOT EXISTS groups (
    group_id      TEXT PRIMARY KEY,
    name          TEXT,
    avatar_url    TEXT,
    owner_id      TEXT,
    member_count  INTEGER,
    my_role       TEXT,
    updated_at_ms INTEGER
);

CREATE TABLE IF NOT EXISTS outbound_queue (
    local_id      TEXT PRIMARY KEY,
    conv_id       TEXT,
    conv_type     TEXT,
    content_type  TEXT,
    content       TEXT,
    retry_count   INTEGER DEFAULT 0,
    created_at    INTEGER
);

CREATE TABLE IF NOT EXISTS metadata (
    key   TEXT PRIMARY KEY,
    value TEXT
);
)sql";

// Apply migration to version 1.
static bool migrateToV1(sqlite3* db) {
    if (!execRaw(db, "BEGIN"))
        return false;
    if (!execRaw(db, kSchemav1)) {
        execRaw(db, "ROLLBACK");
        return false;
    }
    // Update the user_version pragma inside the transaction.
    if (!execRaw(db, "PRAGMA user_version = 1")) {
        execRaw(db, "ROLLBACK");
        return false;
    }
    if (!execRaw(db, "COMMIT")) {
        execRaw(db, "ROLLBACK");
        return false;
    }
    return true;
}

} // anonymous namespace

bool runMigrations(sqlite3* db) {
    int ver = getUserVersion(db);
    if (ver < 0)
        return false;

    if (ver < 1) {
        if (!migrateToV1(db))
            return false;
        ver = 1;
    }

    (void) ver;
    return true;
}

} // namespace anychat::db
