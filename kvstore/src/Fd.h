#pragma once
#include <unistd.h>

#include <cstddef>
#include <string>

// wrapper for RAII purposes only
class Fd {
public:
    explicit Fd(int fd = -1): fd_(fd) {}
    ~Fd() {
        close();
    }

    int fd() const {
        return fd_;
    }

    // only 1 Fd can own file descriptor
    Fd(const Fd& other) = delete;
    Fd& operator=(const Fd& other) = delete;

    Fd(Fd&& other) noexcept: fd_(other.fd_) {
        other.fd_ = -1;
    }

    Fd& operator=(Fd&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
        return *this;
    }

    bool close() {
        bool ok = true;
        if (fd_ >= 0) {
            ok &= ::close(fd_) >= 0;
        }
        fd_ = -1;
        return ok;
    }
private:
    int fd_;
};

bool syncFd(int fd);

bool sendAll(int fd, const std::string& data);
