#include "Store.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "LruList.h"

constexpr int maxWorkPerSweep = 1000;

Store::Store(std::size_t maxKeys): maxKeys_(maxKeys), sweeper_([this] {
    while (running_) {
        if (sweepExpired()) {
            // tell scheduler to allow core to run other threads
            std::this_thread::yield();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}) {}

Store::~Store() {
    running_ = false;
    sweeper_.join();
}

// returns `true` if sweep finished only due to `maxWorkPerSweep`
bool Store::sweepExpired() {
    std::lock_guard<std::mutex> lock(mu_);
    auto now = Clock::now();
    int popped = 0;

    while (!heap_.empty() && heap_.top().when <= now) {
        // break up long backlogs using `maxWorkPerSweep`
        // so it doesn't hold `mu_` for too long
        if (popped == maxWorkPerSweep) {
            return true;
        }

        Expiry e = heap_.top();
        heap_.pop();
        popped++;

        Entry* entry = data_.find(e.key);
        // check if stale due to EXPIRE or PERSIST
        // or SET/SETNX clearing TTLs
        if (entry && entry->expiresAt == e.when) {
            removeEntry(e.key);
        }
    }
    return false;
}

bool Store::isExpired(const Entry& v, TimePoint now) {
    return v.expiresAt && *v.expiresAt <= now;
}

const Store::Entry* Store::findLive(const std::string& key, TimePoint now) const {
    const Entry* v = data_.find(key);
    if (!v || isExpired(*v, now)) {
        return nullptr;
    }
    return v;
}

Store::Entry* Store::findLive(const std::string& key, TimePoint now) {
    return const_cast<Entry*>(std::as_const(*this).findLive(key, now));
}

// precondition: `key` must not already exist in data_
// since it creates a new LRU node
// otherwise existing Entry(s) would be replaced while its node is still linked (dangling)
void Store::insertNew(const std::string& key, std::string value) {
    trie_.insert(key);
    if (maxKeys_ > 0 && data_.size() >= maxKeys_) {
        // copy since removeEntry frees the node that owns this string
        std::string victim = lru_.back()->key;
        removeEntry(victim);
    }
    auto node = std::make_unique<LruList::Node>(key);
    lru_.pushFront(node.get());
    data_.set(key, Entry {std::move(value), std::move(node)});
}

// returns whether a live key was removed
bool Store::removeEntry(const std::string& key) {
    trie_.erase(key);
    bool exists = findLive(key, Clock::now()) != nullptr;
    Entry* v = data_.find(key);
    if (v) {
        lru_.unlink(v->node.get());
    }
    data_.erase(key);
    return exists;
}

void Store::set(const std::string& key, std::string value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (Entry* v = data_.find(key)) {
        v->value = std::move(value);
        v->expiresAt = std::nullopt;
        lru_.moveToFront(v->node.get());
        return;
    }
    insertNew(key, std::move(value));
}

bool Store::setIfAbsent(const std::string& key, std::string value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (findLive(key, Clock::now())) {
        return false;
    }
    removeEntry(key);
    insertNew(key, std::move(value));
    return true;
}

std::optional<std::string> Store::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mu_);
    const Entry* v = findLive(key, Clock::now());
    if (!v) {
        return std::nullopt;
    }
    lru_.moveToFront(v->node.get());
    return v->value;
}

bool Store::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mu_);
    return removeEntry(key);
}

bool Store::exists(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mu_);
    return findLive(key, Clock::now()) != nullptr;
}

std::size_t Store::size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return data_.size();
}

bool Store::expire(const std::string& key, std::chrono::seconds s) {
    std::lock_guard<std::mutex> lock(mu_);
    TimePoint now = Clock::now();
    Entry* v = findLive(key, now);
    if (!v) {
        return false;
    }
    TimePoint exp = now + s;
    v->expiresAt = exp;
    heap_.push(Expiry {key, exp});
    return true;
}

long long Store::ttl(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mu_);
    TimePoint now = Clock::now();
    const Entry* v = findLive(key, now);
    if (!v) {
        return -2;
    }
    if (!v->expiresAt) {
        return -1;
    }
    return std::chrono::round<std::chrono::seconds>(*v->expiresAt - now).count();
}

bool Store::persist(const std::string& key) {
    std::lock_guard<std::mutex> lock(mu_);
    Entry* v = findLive(key, Clock::now());
    if (!v || !v->expiresAt) {
        return false;
    }
    v->expiresAt = std::nullopt;
    return true;
}

std::vector<std::string> Store::keysWithPrefix(const std::string& prefix) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> keys = trie_.collect(prefix);
    TimePoint now = Clock::now();
    std::erase_if(keys, [this, now](const std::string& key) {
        return findLive(key, now) == nullptr;
    });
    return keys;
}