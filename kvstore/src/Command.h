#pragma once
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "Reply.h"
#include "Store.h"
#include "Tokenize.h"

class Command {
public:
    virtual ~Command() = default;
    virtual Reply execute(Store& store, const Args& args) = 0;
};

class GetCommand : public Command {
public:
    Reply execute(Store& store, const Args& args) override;
};

class SetCommand : public Command {
public:
    Reply execute(Store& store, const Args& args) override;
};

class SetNXCommand : public Command {
public:
    Reply execute(Store& store, const Args& args) override;
};

class DelCommand : public Command {
public:
    Reply execute(Store& store, const Args& args) override;
};

class ExistsCommand : public Command {
public:
    Reply execute(Store& store, const Args& args) override;
};

class CountCommand : public Command {
public:
    Reply execute(Store& store, const Args& args) override;
};

class ExpireCommand : public Command {
public:
    Reply execute(Store& store, const Args& args) override;
};

class TTLCommand : public Command {
public:
    Reply execute(Store& store, const Args& args) override;
};

class PersistCommand : public Command {
public:
    Reply execute(Store& store, const Args& args) override;
};

class KeysCommand : public Command {
public:
    Reply execute(Store& store, const Args& args) override;
};

using CommandTable = std::unordered_map<std::string, std::unique_ptr<Command>>;

CommandTable makeCommands();
std::optional<Reply> handleLine(Store& store, const CommandTable& commands, const std::string& line);
