#include "Reply.h"
#include <cstddef>

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
}