#include <string>
#include <iostream>
#include "Store.h"
#include "Command.h"
#include "Reply.h"
#include "Server.h"

int main(int argc, char* argv[]) {
    bool server = false;
    std::size_t maxKeys = 0;

    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--server") {
            server = true;
        } else if (arg == "--maxkeys") {
            if (i + 1 < argc) {
                auto n = parseInt(argv[i + 1]);
                if (n && *n >= 0) {
                    maxKeys = static_cast<std::size_t>(*n);
                    i++;
                    continue;
                }
            }
            std::cerr << "ERR usage: --maxkeys <count>" << std::endl;
            return 1;
        } else {
            std::cerr << "ERR usage: [--server] [--maxkeys <count>]" << std::endl;
            return 1;
        }
    }

    Store store(maxKeys);
    CommandTable t = makeCommands();

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
