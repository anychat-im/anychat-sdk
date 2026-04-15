#pragma once

// Forward-declare sqlite3 to avoid pulling in sqlite3.h in consumer headers.
struct sqlite3;

namespace anychat {
namespace db {

// The current schema version.  Increment this (and add a migration block in
// migrations.cpp) whenever the schema changes.
static constexpr int kCurrentSchemaVersion = 1;

// Apply all pending schema migrations to `db`.
// Returns true on success, false on any error.
bool runMigrations(struct sqlite3* db);

} // namespace db
} // namespace anychat
