#include "Store.h"

void Store::set(const std::string& key, const std::string& value) {
    data_.set(key, Entry {value});
}

std::optional<std::string> Store::get(const std::string& key) const {
    const Entry* e = data_.find(key);
    if (!e) {
        return std::nullopt;
    }
    return e->value;
}

bool Store::del(const std::string& key) {
    return removeEntry(key);
}

bool Store::exists(const std::string& key) const {
    return data_.contains(key);
}

std::size_t Store::size() const {
    return data_.size();
}

bool Store::removeEntry(const std::string& key) {
    return data_.erase(key);
}
