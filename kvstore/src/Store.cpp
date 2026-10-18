#include "Store.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "Fd.h"
#include "LruList.h"
#include "Tokenize.h"

constexpr std::uintmax_t minResizeAofSize = 65536; // 64kB
constexpr int maxWorkPerSweep = 1000;
// backoff on failure to avoid retry spam
constexpr std::chrono::duration rewriteRetryAfter = std::chrono::seconds(60);

static long long ttlToTime(Store::Clock::duration s) {
    return std::chrono::round<std::chrono::seconds>(std::chrono::system_clock::now() + s).time_since_epoch().count();
}

static bool overflowsLL(long long n, long long delta) {
    if (delta > 0) {
        return n > LLONG_MAX - delta;
    } else if (delta < 0) {
        return n < LLONG_MIN - delta;
    }
    return false;
}

static std::string formatRecord(const std::string& cmd, std::initializer_list<std::string_view> args) {
    std::string text = cmd;
    for (std::string_view arg : args) {
        text += " ";
        text += quote(std::string(arg));
    }
    return text;
}

Store::Store(std::size_t maxKeys, std::string aofPath, std::uintmax_t aofGrowth):
    maxKeys_(maxKeys), aofPath_(std::move(aofPath)), aofGrowth_(aofGrowth), sweeper_([this] {
        while (running_) {
            if (sweepExpired()) {
                // tell scheduler to allow core to run other threads
                std::this_thread::yield();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            rewriteAof(false);
        }
    }) {}

Store::~Store() {
    running_ = false;
    sweeper_.join();
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

std::vector<std::optional<std::string>> Store::mget(std::span<const std::string> keys) {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::optional<std::string>> out;
    TimePoint now = Clock::now();
    for (const std::string& key : keys) {
        if (Entry* v = findLive(key, now)) {
            out.push_back(v->value);
            lru_.moveToFront(v->node.get());
        } else {
            out.push_back(std::nullopt);
        }
    }
    return out;
}

void Store::set(const std::string& key, std::string value) {
    std::lock_guard<std::mutex> lock(mu_);
    setLocked(key, std::move(value));
}

void Store::mset(std::span<const std::string> pairs) {
    std::lock_guard<std::mutex> lock(mu_);
    for (std::size_t i = 0; i + 1 < pairs.size(); i += 2) {
        setLocked(pairs[i], pairs[i + 1]);
    }
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

IncrResponse Store::incrBy(const std::string& key, long long delta) {
    std::lock_guard<std::mutex> lock(mu_);
    Entry* v = findLive(key, Clock::now());
    long long res;
    if (!v) {
        removeEntry(key);
        insertNew(key, std::to_string(delta));
        res = delta;
    } else {
        auto n = parseInt<long long>(v->value);
        if (!n) {
            return IncrResponse {IncrStatus::NotInteger};
        }
        if (overflowsLL(*n, delta)) {
            return IncrResponse {IncrStatus::Overflow};
        }
        res = *n + delta;
        v->value = std::to_string(res);
        lru_.moveToFront(v->node.get());
        logRecord("INCRBY", {key, std::to_string(delta)});
    }
    return IncrResponse {
        IncrStatus::Ok,
        res,
    };
}

std::size_t Store::append(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mu_);
    Entry* v = findLive(key, Clock::now());
    if (!v) {
        removeEntry(key);
        insertNew(key, value);
        return value.size();
    }
    v->value += value;
    lru_.moveToFront(v->node.get());
    logRecord("APPEND", {key, value});
    return v->value.size();
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

std::vector<std::string> Store::keysWithPrefix(const std::string& prefix) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> keys = trie_.collect(prefix);
    TimePoint now = Clock::now();
    std::erase_if(keys, [this, now](const std::string& key) {
        return findLive(key, now) == nullptr;
    });
    return keys;
}

void Store::clear() {
    std::lock_guard<std::mutex> lock(mu_);
    lru_.clear();
    trie_.clear();
    heap_.clear();
    data_.clear();
    logRecord("FLUSHALL", {});
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

    logRecord("EXPIREAT", {key, std::to_string(ttlToTime(s))});
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
    logRecord("PERSIST", {key});
    return true;
}

void Store::beginLoading() {
    std::lock_guard<std::mutex> lock(mu_);
    loading_ = true;
}

bool Store::openAof() {
    std::lock_guard<std::mutex> lock(mu_);
    loading_ = false;
    aof_ = Fd(open(aofPath_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644));
    std::error_code ec;
    std::uintmax_t size = std::filesystem::file_size(aofPath_, ec);
    if (!ec) {
        prevAofSize_ = size;
    }
    return aof_.isOpen();
}

bool Store::rewriteAof(bool force) {
    if (aofPath_.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mu_);

    std::error_code ec;
    TimePoint now = Clock::now();
    if (!force) {
        if (now < autoRewriteRetryAt_ || !aof_.isOpen() || !aofGrowth_) {
            return false;
        }
        std::uintmax_t size = std::filesystem::file_size(aofPath_, ec);
        if (ec || size < minResizeAofSize || size < aofGrowth_ * prevAofSize_) {
            return false;
        }
    }

    std::string tmpPath = aofPath_ + ".tmp";
    Fd tmp(open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644));

    auto fail = [&](const char* step, const std::string& reason) {
        std::cerr << "aof rewrite: " << step << ": " << reason << std::endl;
        tmp.close();
        std::error_code ignored;
        std::filesystem::remove(tmpPath, ignored);
        if (!force) {
            autoRewriteRetryAt_ = now + rewriteRetryAfter;
        }
        return false;
    };

    if (!tmp.isOpen()) {
        return fail("open", std::strerror(errno));
    }

    std::uintmax_t newSize = 0;
    for (const auto& [k, v] : data_) {
        if (isExpired(v, now)) {
            continue;
        }
        std::string text = formatRecord("SET", {k, v.value}) + "\n";
        if (v.expiresAt) {
            text += formatRecord("EXPIREAT", {k, std::to_string(ttlToTime(*v.expiresAt - now))}) + "\n";
        }
        if (!sendAll(tmp.fd(), text)) {
            return fail("write", std::strerror(errno));
        }
        newSize += text.size();
    }
    if (!syncFd(tmp.fd())) {
        return fail("fsync", std::strerror(errno));
    }
    if (!tmp.close()) {
        return fail("close", std::strerror(errno));
    }

    aof_.close();
    std::filesystem::rename(tmpPath, aofPath_, ec);
    aof_ = Fd(open(aofPath_.c_str(), O_WRONLY | O_APPEND, 0644));
    if (!aof_.isOpen()) {
        perror("aof reopen");
    }
    if (ec) {
        return fail("rename", ec.message());
    }
    prevAofSize_ = newSize;
    autoRewriteRetryAt_ = {};
    return aof_.isOpen();
}

// returns `true` if sweep finished only due to `maxWorkPerSweep`
bool Store::sweepExpired() {
    std::lock_guard<std::mutex> lock(mu_);
    if (loading_) {
        return false;
    }
    auto now = Clock::now();

    if (now - lastSync_ >= std::chrono::seconds(1)) {
        if (!syncFd(aof_.fd())) {
            perror("aof fsync");
        }
        lastSync_ = now;
    }
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
    if (!v || (!loading_ && isExpired(*v, now))) {
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
    logRecord("SET", {key, value});
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
    if (data_.erase(key)) {
        logRecord("DEL", {key});
    }
    return exists;
}

void Store::setLocked(const std::string& key, std::string value) {
    if (Entry* v = data_.find(key)) {
        v->value = std::move(value);
        v->expiresAt = std::nullopt;
        lru_.moveToFront(v->node.get());
        logRecord("SET", {key, v->value});
        return;
    }
    insertNew(key, std::move(value));
}

void Store::logRecord(const std::string& cmd, std::initializer_list<std::string_view> args) {
    if (!aof_.isOpen()) {
        return;
    }
    struct stat st;
    bool haveSize = fstat(aof_.fd(), &st) == 0;

    if (!sendAll(aof_.fd(), formatRecord(cmd, args) + "\n")) {
        perror("aof");
        // truncate a half-written line to the old size before write if write fails
        if (haveSize && ftruncate(aof_.fd(), st.st_size) < 0) {
            perror("aof truncate");
        }
    }
}
