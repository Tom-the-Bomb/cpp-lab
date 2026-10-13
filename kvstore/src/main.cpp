#include <string>
#include <iostream>
#include "Store.h"
#include "Command.h"
#include "Reply.h"

int main() {
    std::string line;
    Store store;
    CommandTable t = makeCommands();

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
