#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

#include "Reply.h"
#include "Store.h"
#include "Tokenize.h"

struct Command {
    Reply (*run)(Store& store, const Args& args);
    std::size_t minArgs;
    std::size_t maxArgs;
    const char* usage;
};

using CommandTable = std::unordered_map<std::string, Command>;

std::optional<Reply> handleArgs(Store& store, const CommandTable& commands, const Args& args);
std::optional<Reply> handleLine(Store& store, const CommandTable& commands, const std::string& line);

CommandTable makeCommands();
