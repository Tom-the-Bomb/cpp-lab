
#include "Fd.h"

#include <unistd.h>

#include <cstddef>
#include <string>

bool syncFd(int fd) {
    if (fd < 0) {
        return true;
    }
    return ::fsync(fd) >= 0;
}

bool sendAll(int fd, const std::string& data) {
    std::size_t len = data.size();
    std::size_t start = 0;
    while (len > 0) {
        // send may only send some parts at the start
        ssize_t sent = write(fd, data.data() + start, len);
        // -1 = error, 0 = no bytes send (inf loop)
        if (sent <= 0) {
            return false;
        }
        len -= sent;
        start += sent;
    }
    return true;
}
