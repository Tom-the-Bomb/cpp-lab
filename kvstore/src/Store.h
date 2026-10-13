#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include "HashMap.h"

class Store {
public:
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    bool exists(const std::string& key) const;
    std::size_t size() const;
private:
    struct Entry {
        std::string value;
    };

    HashMap<std::string, Entry, FnvHash> data_;

    bool removeEntry(const std::string& key);
};