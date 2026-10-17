#include "Command.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Reply.h"
#include "Store.h"
#include "Tokenize.h"

// 100 years
constexpr long long maxSecs = 100LL * 365 * 24 * 60 * 60;

static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return std::toupper(c);
    });
    return s;
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
    t["EXPIREAT"] = std::make_unique<ExpireAtCommand>();
    t["TTL"] = std::make_unique<TTLCommand>();
    t["PERSIST"] = std::make_unique<PersistCommand>();
    t["KEYS"] = std::make_unique<KeysCommand>();
    t["REWRITEAOF"] = std::make_unique<RewriteAOFCommand>();
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
    // -ve values delete keys
    if (*secs > maxSecs || *secs < -maxSecs) {
        return Reply::error("ERR invalid expire time in 'expire' command");
    }
    return Reply::integer(store.expire(args[1], std::chrono::seconds(*secs)));
}

Reply ExpireAtCommand::execute(Store& store, const Args& args) {
    if (args.size() != 3) {
        return Reply::error("ERR usage: EXPIREAT <key> <unix seconds>");
    }
    std::optional<long long> at = parseInt(args[2]);
    if (!at) {
        return Reply::error("ERR value is not an integer or out of range");
    }
    long long now = std::chrono::round<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    long long remaining = *at <= now ? 0 : *at - now; // turn all past expiries to 0 to avoid overflow issues
    if (remaining > maxSecs) {
        return Reply::error("ERR invalid expire time in 'expireat' command");
    }
    return Reply::integer(store.expire(args[1], std::chrono::seconds(remaining)));
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

Reply KeysCommand::execute(Store& store, const Args& args) {
    if (args.size() != 2) {
        return Reply::error("ERR usage: KEYS <prefix>");
    }
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

Reply RewriteAOFCommand::execute(Store& store, const Args& args) {
    if (args.size() != 1) {
        return Reply::error("ERR usage: REWRITEAOF");
    }
    return Reply::integer(store.rewriteAof());
}
