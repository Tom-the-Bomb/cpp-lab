#pragma once
#include <string>
#include <utility>
#include <vector>

struct Reply {
    // follows RESP (REdis Serialization Protocol)
    enum class Type {
        Simple,
        Error,
        Integer,
        Bulk,
        Nil,
        Array
    };

    Type type = Type::Nil;
    std::string str;

    long long num = 0;
    std::vector<Reply> items;

    static Reply simple(std::string str) {
        return Reply {
            .type = Type::Simple,
            .str = std::move(str),
        };
    }

    static Reply ok() {
        return simple("OK");
    }

    static Reply error(std::string error) {
        return Reply {
            .type = Type::Error,
            .str = std::move(error),
        };
    }

    static Reply integer(long long num) {
        return Reply {
            .type = Type::Integer,
            .num = num,
        };
    }

    static Reply bulk(std::string str) {
        return Reply {
            .type = Type::Bulk,
            .str = std::move(str),
        };
    }

    static Reply nil() {
        return Reply {
            .type = Type::Nil,
        };
    }

    static Reply array(std::vector<Reply> items) {
        return Reply {
            .type = Type::Array,
            .items = std::move(items),
        };
    }
};

// used for REPL, nc
std::string toText(const Reply& r);
