#pragma once
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include "HashMap.h"
#include "MinHeap.h"

class Store {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    Store();
    ~Store();

    // store is non-copyable due to mutex and thread
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    // lock `mu_` themselves
    void set(const std::string& key, std::string value);
    bool setIfAbsent(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    bool exists(const std::string& key) const;
    bool expire(const std::string& key, std::chrono::seconds s);
    long long ttl(const std::string& key) const;
    bool persist(const std::string& key);
    std::size_t size() const;
private:
    struct Entry {
        std::string value;
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
    std::atomic<bool> running_ = true;
    std::thread sweeper_;

    // callers hold `mu_`
    static bool isExpired(const Entry& v, TimePoint now);
    const Entry* findLive(const std::string& key, TimePoint now) const;
    Entry* findLive(const std::string& key, TimePoint now);
    bool removeEntry(const std::string& key);

    bool sweepExpired();
};