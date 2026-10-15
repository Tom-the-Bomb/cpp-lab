#include "Command.h"
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <charconv>
#include <chrono>

static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return std::toupper(c);
    });
    return s;
}

// stoll() but according to Redis behavior
static std::optional<long long> parseInt(const std::string& s) {
    long long n;
    // ptr to 1 past the last character
    const char* end = s.data() + s.size();
    // `ptr` is where it stopped, `s` being int means `ptr` should be 1 past the last character
    // otherwise something like `10abc` was parsed
    // `ec` is error code, `errc` is success
    auto [ptr, ec] = std::from_chars(s.data(), end, n);
    if (ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return n;
}

std::optional<Reply> handleLine(Store& store, const CommandTable& commands, const std::string& line) {
    Args args;
    try {
        args = tokenize(line);
    } catch (const std::invalid_argument&) {
        return Reply::error("ERR Protocol error: unbalanced quotes in request");
    }

    if (args.empty()) {
        return std::nullopt;
    }
    auto it = commands.find(toUpper(args[0]));
    if (it == commands.end()) {
        return Reply::error("ERR unknown command '" + args[0] + "'");
    }
    return it->second->execute(store, args);
}

CommandTable makeCommands() {
    CommandTable t;
    t["GET"] = std::make_unique<GetCommand>();
    t["SET"] = std::make_unique<SetCommand>();
    t["SETNX"] = std::make_unique<SetNXCommand>();
    t["DEL"] = std::make_unique<DelCommand>();
    t["EXISTS"] = std::make_unique<ExistsCommand>();
    t["COUNT"] = std::make_unique<CountCommand>();
    t["EXPIRE"] = std::make_unique<ExpireCommand>();
    t["TTL"] = std::make_unique<TTLCommand>();
    t["PERSIST"] = std::make_unique<PersistCommand>();
    return t;
}

Reply GetCommand::execute(Store& store, const Args& args) {
    if (args.size() != 2) {
        return Reply::error("ERR usage: GET <key>");
    }
    auto v = store.get(args[1]);
    if (!v) {
        return Reply::nil();
    }
    return Reply::bulk(std::move(*v));
}

Reply SetCommand::execute(Store& store, const Args& args) {
    if (args.size() != 3) {
        return Reply::error("ERR usage: SET <key> <value>");
    }
    store.set(args[1], args[2]);
    return Reply::ok();
}

Reply SetNXCommand::execute(Store& store, const Args& args) {
    if (args.size() != 3) {
        return Reply::error("ERR usage: SETNX <key> <value>");
    }
    return Reply::integer(store.setIfAbsent(args[1], args[2]));
}

Reply DelCommand::execute(Store& store, const Args& args) {
    if (args.size() < 2) {
        return Reply::error("ERR usage: DEL <...keys>");
    }
    int cnt = 0;
    for (std::size_t i = 1; i < args.size(); i++) {
        cnt += store.del(args[i]);
    }
    return Reply::integer(cnt);
}

Reply ExistsCommand::execute(Store& store, const Args& args) {
    if (args.size() < 2) {
        return Reply::error("ERR usage: EXISTS <...keys>");
    }
    int cnt = 0;
    for (std::size_t i = 1; i < args.size(); i++) {
        cnt += store.exists(args[i]);
    }
    return Reply::integer(cnt);
}

Reply CountCommand::execute(Store& store, const Args& args) {
    if (args.size() != 1) {
        return Reply::error("ERR usage: COUNT");
    }
    return Reply::integer(store.size());
}

Reply ExpireCommand::execute(Store& store, const Args& args) {
    if (args.size() != 3) {
        return Reply::error("ERR usage: EXPIRE <key> <seconds>");
    }
    std::optional<long long> secs = parseInt(args[2]);
    if (!secs) {
        return Reply::error("ERR value is not an integer or out of range");
    }
    // 100 years
    constexpr long long maxSecs = 100LL * 365 * 24 * 60 * 60;
    // -ve values delete keys
    if (*secs > maxSecs || *secs < -maxSecs) {
        return Reply::error("ERR invalid expire time in 'expire' command");
    }
    return Reply::integer(store.expire(args[1], std::chrono::seconds(*secs)));
}

Reply TTLCommand::execute(Store& store, const Args& args) {
    if (args.size() != 2) {
        return Reply::error("ERR usage: TTL <key>");
    }
    return Reply::integer(store.ttl(args[1]));
}

Reply PersistCommand::execute(Store& store, const Args& args) {
    if (args.size() != 2) {
        return Reply::error("ERR usage: PERSIST <key>");
    }
    return Reply::integer(store.persist(args[1]));
}