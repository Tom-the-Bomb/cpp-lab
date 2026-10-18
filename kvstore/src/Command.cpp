#include "Command.h"

#include <chrono>
#include <climits>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Reply.h"
#include "Store.h"
#include "Tokenize.h"

constexpr long long maxSecs = 100LL * 365 * 24 * 60 * 60; // 100 years
constexpr std::size_t variadic = std::numeric_limits<std::size_t>::max();
constexpr std::size_t maxKeySize = 256;

static bool keyTooLong(const std::string& key) {
    return key.size() > maxKeySize;
}

static Reply keyTooLongError() {
    return Reply::error("ERR key too long (max " + std::to_string(maxKeySize) + " bytes)");
}

std::optional<Reply> handleArgs(Store& store, const CommandTable& commands, const Args& args) {
    if (args.empty()) {
        return std::nullopt;
    }
    auto it = commands.find(toUpper(args[0]));
    if (it == commands.end()) {
        return Reply::error("ERR unknown command '" + args[0] + "'");
    }
    const Command& cmd = it->second;
    if (args.size() < cmd.minArgs || args.size() > cmd.maxArgs) {
        return Reply::error(std::string("ERR usage: ") + cmd.usage);
    }
    return cmd.run(store, args);
}

std::optional<Reply> handleLine(Store& store, const CommandTable& commands, const std::string& line) {
    Args args;
    try {
        args = tokenize(line);
    } catch (const std::invalid_argument&) {
        return Reply::error("ERR Protocol error: unbalanced quotes in request");
    }
    return handleArgs(store, commands, args);
}

static Reply ping(Store&, const Args& args) {
    if (args.size() == 2) {
        return Reply::bulk(args[1]);
    }
    return Reply::simple("PONG");
}

static Reply echo(Store&, const Args& args) {
    return Reply::bulk(args[1]);
}

static Reply get(Store& store, const Args& args) {
    auto v = store.get(args[1]);
    if (!v) {
        return Reply::nil();
    }
    return Reply::bulk(std::move(*v));
}

static Reply mget(Store& store, const Args& args) {
    std::vector<Reply> out;
    out.reserve(args.size() - 1);

    auto r = store.mget(std::span(args).subspan(1));
    for (auto& el : r) {
        if (el) {
            out.push_back(Reply::bulk(std::move(*el)));
        } else {
            out.push_back(Reply::nil());
        }
    }
    return Reply::array(std::move(out));
}

static Reply strLen(Store& store, const Args& args) {
    return Reply::integer(get(store, args).str.size());
}

static Reply set(Store& store, const Args& args) {
    if (keyTooLong(args[1])) {
        return keyTooLongError();
    }
    store.set(args[1], args[2]);
    return Reply::ok();
}

static Reply mset(Store& store, const Args& args) {
    if (args.size() % 2 == 0) {
        return Reply::error("ERR usage: MSET <key> <value> [<key> <value>...]");
    }
    for (std::size_t i = 1; i < args.size(); i += 2) {
        if (keyTooLong(args[i])) {
            return keyTooLongError();
        }
    }
    store.mset(std::span(args).subspan(1));
    return Reply::ok();
}

static Reply setNX(Store& store, const Args& args) {
    if (keyTooLong(args[1])) {
        return keyTooLongError();
    }
    return Reply::integer(store.setIfAbsent(args[1], args[2]));
}

static Reply incrReply(Store& store, const std::string& key, long long delta) {
    if (keyTooLong(key)) {
        return keyTooLongError();
    }
    IncrResponse r = store.incrBy(key, delta);
    switch (r.status) {
        case IncrStatus::Ok:
            return Reply::integer(r.n);
        case IncrStatus::NotInteger:
            return Reply::error("ERR value is not an integer or out of range");
        case IncrStatus::Overflow:
            return Reply::error("ERR increment or decrement would overflow");
    }
}

static Reply incr(Store& store, const Args& args) {
    return incrReply(store, args[1], 1);
}

static Reply incrBy(Store& store, const Args& args) {
    if (auto delta = parseInt<long long>(args[2])) {
        return incrReply(store, args[1], *delta);
    }
    return Reply::error("ERR value is not an integer or out of range");
}

static Reply decr(Store& store, const Args& args) {
    return incrReply(store, args[1], -1);
}

static Reply decrBy(Store& store, const Args& args) {
    auto delta = parseInt<long long>(args[2]);
    if (!delta || *delta == LLONG_MIN) {
        return Reply::error("ERR value is not an integer or out of range");
    }
    return incrReply(store, args[1], -*delta);
}

static Reply append(Store& store, const Args& args) {
    if (keyTooLong(args[1])) {
        return keyTooLongError();
    }
    return Reply::integer(store.append(args[1], args[2]));
}

static Reply del(Store& store, const Args& args) {
    int cnt = 0;
    for (std::size_t i = 1; i < args.size(); i++) {
        cnt += store.del(args[i]);
    }
    return Reply::integer(cnt);
}

static Reply exists(Store& store, const Args& args) {
    int cnt = 0;
    for (std::size_t i = 1; i < args.size(); i++) {
        cnt += store.exists(args[i]);
    }
    return Reply::integer(cnt);
}

static Reply dbSize(Store& store, const Args&) {
    return Reply::integer(store.size());
}

static Reply keys(Store& store, const Args& args) {
    std::string pre = args[1];
    std::vector<Reply> out;

    if (pre.ends_with("*")) {
        pre.pop_back();
        std::vector<std::string> match = store.keysWithPrefix(pre);
        for (std::string& k : match) {
            out.push_back(Reply::bulk(std::move(k)));
        }
    } else if (store.exists(pre)) {
        out.push_back(Reply::bulk(std::move(pre)));
    }
    return Reply::array(std::move(out));
}

static Reply flushAll(Store& store, const Args&) {
    store.clear();
    return Reply::ok();
}

static Reply expire(Store& store, const Args& args) {
    std::optional<long long> secs = parseInt<long long>(args[2]);
    if (!secs) {
        return Reply::error("ERR value is not an integer or out of range");
    }
    // -ve values delete keys
    if (*secs > maxSecs || *secs < -maxSecs) {
        return Reply::error("ERR invalid expire time in 'expire' command");
    }
    return Reply::integer(store.expire(args[1], std::chrono::seconds(*secs)));
}

static Reply expireAt(Store& store, const Args& args) {
    std::optional<long long> at = parseInt<long long>(args[2]);
    if (!at) {
        return Reply::error("ERR value is not an integer or out of range");
    }
    long long now =
        std::chrono::round<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    long long remaining = *at <= now ? 0 : *at - now; // turn all past expiries to 0 to avoid overflow issues
    if (remaining > maxSecs) {
        return Reply::error("ERR invalid expire time in 'expireat' command");
    }
    return Reply::integer(store.expire(args[1], std::chrono::seconds(remaining)));
}

static Reply ttl(Store& store, const Args& args) {
    return Reply::integer(store.ttl(args[1]));
}

static Reply persist(Store& store, const Args& args) {
    return Reply::integer(store.persist(args[1]));
}

static Reply rewriteAof(Store& store, const Args&) {
    return Reply::integer(store.rewriteAof(true));
}

CommandTable makeCommands() {
    return {
        {"PING", {ping, 1, 2, "PING [message]"}},
        {"ECHO", {echo, 2, 2, "ECHO <message>"}},
        {"GET", {get, 2, 2, "GET <key>"}},
        {"MGET", {mget, 2, variadic, "MGET <...keys>"}},
        {"STRLEN", {strLen, 2, 2, "STRLEN <key>"}},
        {"SET", {set, 3, 3, "SET <key> <value>"}},
        {"MSET", {mset, 3, variadic, "MSET <key> <value> [<key> <value>...]"}},
        {"SETNX", {setNX, 3, 3, "SETNX <key> <value>"}},
        {"INCR", {incr, 2, 2, "INCR <key>"}},
        {"INCRBY", {incrBy, 3, 3, "INCRBY <key> <delta>"}},
        {"DECR", {decr, 2, 2, "DECR <key>"}},
        {"DECRBY", {decrBy, 3, 3, "DECRBY <key> <delta>"}},
        {"APPEND", {append, 3, 3, "APPEND <key> <value>"}},
        {"DEL", {del, 2, variadic, "DEL <...keys>"}},
        {"EXISTS", {exists, 2, variadic, "EXISTS <...keys>"}},
        {"DBSIZE", {dbSize, 1, 1, "DBSIZE"}},
        {"KEYS", {keys, 2, 2, "KEYS <prefix>"}},
        {"FLUSHALL", {flushAll, 1, 1, "FLUSHALL"}},
        {"EXPIRE", {expire, 3, 3, "EXPIRE <key> <seconds>"}},
        {"EXPIREAT", {expireAt, 3, 3, "EXPIREAT <key> <unix seconds>"}},
        {"TTL", {ttl, 2, 2, "TTL <key>"}},
        {"PERSIST", {persist, 2, 2, "PERSIST <key>"}},
        {"REWRITEAOF", {rewriteAof, 1, 1, "REWRITEAOF"}},
    };
}
