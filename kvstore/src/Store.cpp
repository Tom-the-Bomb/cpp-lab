#include "Store.h"

void Store::set(const std::string& key, std::string value) {
    std::lock_guard<std::mutex> lock(mu_);
    data_.set(key, Entry {std::move(value)});
}

bool Store::setIfAbsent(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (data_.contains(key)) {
        return false;
    }
    data_.set(key, Entry {value});
    return true;
}

std::optional<std::string> Store::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mu_);
    const Entry* e = data_.find(key);
    if (!e) {
        return std::nullopt;
    }
    return e->value;
}

bool Store::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mu_);
    return removeEntry(key);
}

bool Store::exists(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mu_);
    return data_.contains(key);
}

std::size_t Store::size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return data_.size();
}

bool Store::removeEntry(const std::string& key) {
    // caller holds lock
    return data_.erase(key);
}
