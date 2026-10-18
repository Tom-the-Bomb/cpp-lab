#include "Reply.h"

#include <cstddef>
#include <string>

std::string toText(const Reply& r) {
    switch (r.type) {
        case Reply::Type::Simple:
        case Reply::Type::Error:
        case Reply::Type::Bulk:
            return r.str;
        case Reply::Type::Integer:
            return std::to_string(r.num);
        case Reply::Type::Nil:
            return "(nil)";
        case Reply::Type::Array: {
            if (r.items.empty()) {
                return "(empty)";
            }
            std::string out;
            for (std::size_t i = 0; i < r.items.size(); i++) {
                if (i > 0) {
                    out += "\n";
                }
                out += toText(r.items[i]);
            }
            return out;
        }
    }
    return "";
}

std::string toResp(const Reply& r) {
    switch (r.type) {
        case Reply::Type::Simple:
            return "+" + r.str + "\r\n";
        case Reply::Type::Error:
            return "-" + r.str + "\r\n";
        case Reply::Type::Integer:
            return ":" + std::to_string(r.num) + "\r\n";
        case Reply::Type::Bulk:
            return "$" + std::to_string(r.str.size()) + "\r\n" + r.str + "\r\n";
        case Reply::Type::Nil:
            return "$-1\r\n";
        case Reply::Type::Array: {
            std::string out = "*" + std::to_string(r.items.size()) + "\r\n";
            for (const Reply& item : r.items) {
                out += toResp(item);
            }
            return out;
        }
    }
    return "";
}
