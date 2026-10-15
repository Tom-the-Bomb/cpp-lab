#include <string>
#include <iostream>
#include "Store.h"
#include "Command.h"
#include "Reply.h"
#include "Server.h"

int main(int argc, char* argv[]) {
    Store store;
    CommandTable t = makeCommands();

    if (argc == 2 && std::string(argv[1]) == "--server") {
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
