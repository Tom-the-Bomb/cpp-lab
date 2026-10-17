#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

#include "Command.h"
#include "Reply.h"
#include "Server.h"
#include "Store.h"
#include "Tokenize.h"

static bool makeParentDirs(const std::string& path) {
    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (parent.empty()) {
        return true;
    }
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        std::cerr << "aof: " << ec.message() << std::endl;
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    bool server = false;
    std::string aofPath;
    std::uintmax_t aofGrowth = 0;
    std::size_t maxKeys = 0;

    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--server") {
            server = true;
        } else if (arg == "--maxkeys") {
            if (i + 1 < argc) {
                if (auto n = parseInt<std::size_t>(argv[i + 1])) {
                    maxKeys = *n;
                    i++;
                    continue;
                }
            }
            std::cerr << "ERR usage: --maxkeys <count>" << std::endl;
            return 1;
        } else if (arg == "--aof") {
            if (i + 1 < argc) {
                aofPath = argv[i + 1];
                i++;
                continue;
            }
            std::cerr << "ERR usage: --aof <path>" << std::endl;
            return 1;
        } else if (arg == "--aof-resize") {
            if (i + 1 < argc) {
                if (auto n = parseInt<std::uintmax_t>(argv[i + 1])) {
                    aofGrowth = *n;
                    i++;
                    continue;
                }
            }
            std::cerr << "ERR usage: --aof-resize <factor>" << std::endl;
            return 1;
        } else {
            std::cerr << "ERR usage: [--server] [--maxkeys <count>] [--aof <path>] [--aof-resize <factor>]"
                      << std::endl;
            return 1;
        }
    }

    Store store(maxKeys, aofPath, aofGrowth);
    CommandTable t = makeCommands();

    if (!aofPath.empty()) {
        store.beginLoading();
        // replay existing AOF file commands
        if (std::filesystem::exists(aofPath)) {
            std::ifstream in(aofPath);
            if (!in) {
                perror("aof");
                return 1;
            }
            std::string line;
            std::uintmax_t size = 0;
            bool corrupt = false;
            while (std::getline(in, line)) {
                if (in.eof()) {
                    corrupt = true;
                    std::cerr << "aof: dropped incomplete last record" << std::endl;
                    break;
                }
                size += line.size() + 1; // +1 for \n
                handleLine(store, t, line);
            }
            if (corrupt) {
                // other cases RAII handles closing the file
                in.close();
                std::error_code ec;
                std::filesystem::resize_file(aofPath, size, ec);
                if (ec) {
                    std::cerr << "aof: " << ec.message() << std::endl;
                    return 1;
                }
            }
        }
        if (!makeParentDirs(aofPath) || !store.openAof()) {
            perror(("aof: " + aofPath).c_str());
            return 1;
        }
    }

    if (server) {
        // port is 1 above Redis
        return !runServer(store, t, 6380);
    }
    std::string line;

    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) {
            return 0;
        }
        auto out = handleLine(store, t, line);
        if (out) {
            std::cout << toText(*out) << std::endl;
        }
    }
}
