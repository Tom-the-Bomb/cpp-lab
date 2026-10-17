#pragma once
#include <charconv>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

using Args = std::vector<std::string>;

// stoll() but according to Redis behavior
template <typename T> std::optional<T> parseInt(const std::string& s) {
    T n;
    // ptr to 1 past the last character
    const char* end = s.data() + s.size();
    // `ptr` is where it stopped, `s` being int means `ptr` should be 1 past the last character
    // otherwise something like `10abc` was parsed
    // `ec` is error code, `errc` is success
    auto [ptr, ec] = std::from_chars(s.data(), end, n);
    if (ec != std::errc {} || ptr != end) {
        return std::nullopt;
    }
    return n;
}

Args tokenize(const std::string& line);

std::string quote(const std::string& s);
