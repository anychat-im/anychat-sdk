#pragma once

#include "anychat/types.h"

#include <algorithm>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace anychat::cache {

static constexpr size_t kDefaultBucketSize = 100;

// Stores the most recent N messages per conversation, keyed by conv_id.
//
// Each "bucket" is a vector of Messages sorted by seq ascending.  When the
// bucket reaches `bucket_size_`, the message with the lowest seq is evicted.
//
// Provides seq-gap detection so the caller knows whether it must fetch
// offline messages from the server before using the cached data.
//
// All public methods are thread-safe.
class MessageCache {
public:
    explicit MessageCache(size_t bucket_size = kDefaultBucketSize)
        : bucket_size_(bucket_size) {}

    // Add a message to its conversation's bucket.
    // If the bucket is full, the message with the lowest seq is evicted.
    // Duplicate message_ids are silently ignored.
    void insert(const Message& msg) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& bucket = buckets_[msg.conv_id];

        // Dedup by message_id.
        for (const auto& m : bucket) {
            if (m.message_id == msg.message_id)
                return;
        }

        bucket.push_back(msg);

        // Keep sorted by seq ascending.
        std::sort(bucket.begin(), bucket.end(), [](const Message& a, const Message& b) {
            return a.seq < b.seq;
        });

        // Evict the oldest (lowest-seq) entry when over capacity.
        if (bucket.size() > bucket_size_) {
            bucket.erase(bucket.begin());
        }
    }

    // Return a sorted-by-seq-ascending copy of all cached messages for a
    // conversation.  Returns an empty vector if the conv is unknown.
    std::vector<Message> get(const std::string& conv_id) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = buckets_.find(conv_id);
        if (it == buckets_.end())
            return {};
        return it->second; // already sorted
    }

    // Return the maximum (highest) seq seen for a conversation.
    // Returns 0 if no messages are cached for that conversation.
    int64_t maxSeq(const std::string& conv_id) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = buckets_.find(conv_id);
        if (it == buckets_.end() || it->second.empty())
            return 0;
        return it->second.back().seq; // back = highest seq (sorted asc)
    }

    // Returns true if there is a gap before `seq` in the cached sequence for
    // this conversation.  A gap is defined as: the message whose seq
    // immediately precedes `seq` in the ideal sequence is absent from the
    // cache.
    //
    // Concretely: if the highest cached seq + 1 < seq (and the cache is
    // non-empty), there is a gap.  If the cache is empty and seq > 1, that
    // also indicates a gap.
    bool hasGapBefore(const std::string& conv_id, int64_t seq) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = buckets_.find(conv_id);
        if (it == buckets_.end() || it->second.empty()) {
            // No cached messages — gap exists unless seq is the first message.
            return seq > 1;
        }
        int64_t highest = it->second.back().seq;
        // Expected next seq is highest + 1.  If incoming seq is further ahead,
        // there is a gap.
        return seq > highest + 1;
    }

    // Remove all cached messages for the given conversation.
    void removeConversation(const std::string& conv_id) {
        std::lock_guard<std::mutex> lk(mutex_);
        buckets_.erase(conv_id);
    }

    // Update a message in place by message_id. Returns true when found.
    bool updateMessageById(const std::string& message_id, const std::function<void(Message&)>& updater) {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& entry : buckets_) {
            auto& bucket = entry.second;
            for (auto& msg : bucket) {
                if (msg.message_id == message_id) {
                    updater(msg);
                    return true;
                }
            }
        }
        return false;
    }

    // Clear all buckets.
    void clear() {
        std::lock_guard<std::mutex> lk(mutex_);
        buckets_.clear();
    }

private:
    size_t bucket_size_;
    mutable std::mutex mutex_;

    // conv_id → messages sorted by seq ascending.
    std::unordered_map<std::string, std::vector<Message>> buckets_;
};

} // namespace anychat::cache
