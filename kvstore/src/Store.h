#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "Fd.h"
#include "HashMap.h"
#include "LruList.h"
#include "MinHeap.h"
#include "Trie.h"

enum class IncrStatus {
    Ok,
    NotInteger,
    Overflow,
};

struct IncrResponse {
    IncrStatus status;
    long long n = 0;
};

class Store {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit Store(std::size_t maxKeys = 0, std::string aofPath = "", std::uintmax_t = 0);
    ~Store();

    // store is non-copyable due to mutex and thread
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    // lock `mu_` themselves
    void set(const std::string& key, std::string value);
    bool setIfAbsent(const std::string& key, std::string value);
    IncrResponse incrBy(const std::string& key, long long delta);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key) const;
    std::size_t size() const;
    std::vector<std::optional<std::string>> mget(std::span<const std::string> keys);
    void mset(std::span<const std::string> pairs);
    bool expire(const std::string& key, std::chrono::seconds s);
    long long ttl(const std::string& key) const;
    bool persist(const std::string& key);
    std::vector<std::string> keysWithPrefix(const std::string& prefix) const;
    bool openAof();
    void beginLoading();
    bool rewriteAof(bool force);
private:
    struct Entry {
        std::string value;
        std::unique_ptr<LruList::Node> node;
        std::optional<TimePoint> expiresAt = std::nullopt;
    };

    struct Expiry {
        std::string key;
        TimePoint when;
    };

    struct ExpiryLess {
        bool operator()(const Expiry& a, const Expiry& b) const {
            return a.when < b.when;
        }
    };

    mutable std::mutex mu_;
    HashMap<std::string, Entry, FnvHash> data_;
    MinHeap<Expiry, ExpiryLess> heap_;
    LruList lru_;
    Trie trie_;
    std::size_t maxKeys_;
    std::atomic<bool> running_ = true;
    // the store is currently being rebuilt from the AOF
    // used to temporarily pause expiration checks
    bool loading_ = false; // guarded by `mu_`
    Fd aof_;
    TimePoint lastSync_; // sweeper thread
    const std::string aofPath_;
    const std::uintmax_t aofGrowth_;
    std::uintmax_t prevAofSize_ = 0;
    TimePoint autoRewriteRetryAt_;
    std::thread sweeper_; // `sweeper_` needs to be declared last since threads start immediately

    bool sweepExpired();

    // callers hold `mu_`
    static bool isExpired(const Entry& v, TimePoint now);
    const Entry* findLive(const std::string& key, TimePoint now) const;
    Entry* findLive(const std::string& key, TimePoint now);
    void setLocked(const std::string& key, std::string value);
    void insertNew(const std::string& key, std::string value);
    bool removeEntry(const std::string& key);
    void logRecord(const std::string& cmd, std::initializer_list<std::string_view> args);
};
