#include <arpa/inet.h>
#include <csignal>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <string>
#include "Server.h"
#include "Reply.h"

class Socket {
public:
    explicit Socket(int fd): fd_(fd) {}
    ~Socket() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    int fd() const {
        return fd_;
    }

    // only 1 socket can own while file descriptor
    Socket(const Socket& other) = delete;
    Socket& operator=(const Socket& other) = delete;

    Socket(Socket&& other) noexcept: fd_(other.fd_) {
        other.fd_ = -1;
    }

    Socket& operator=(Socket&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (fd_ >= 0) {
            close(fd_);
        }
        fd_ = other.fd_;
        other.fd_ = -1;
        return *this;
    }
private:
    int fd_;
};

static bool sendAll(int fd, const std::string& data) {
    std::size_t len = data.size();
    std::size_t start = 0;
    while (len > 0) {
        // send may only send some parts at the start
        ssize_t sent = send(fd, data.data() + start, len, 0);
        // -1 = error, 0 = no bytes send (inf loop)
        if (sent <= 0) {
            return false;
        }
        len -= sent;
        start += sent;
    }
    return true;
}

static void handleClient(int fd, Store& store, const CommandTable& table) {
    std::string buf;
    // 16kB
    char chunk[4096];

    while (true) {
        ssize_t n = recv(fd, chunk, sizeof(chunk), 0);
        // -1 = error, 0 = connection closed
        if (n <= 0) {
            return;
        }
        buf.append(chunk, n);

        std::size_t i;
        while ((i = buf.find('\n')) != buf.npos) {
            std::string line = buf.substr(0, i);
            buf.erase(0, i + 1);
            if (line.ends_with('\r')) {
                line.pop_back();
            }
            auto reply = handleLine(store, table, line);
            if (reply) {
                if (!sendAll(fd, toText(*reply) + "\n")) {
                    return;
                }
            }
        }
    }
}

bool runServer(Store& store, const CommandTable& commands, int port) {
    // signal ignore SIGPIPE
    // normally `send` would crash program if writing to broken conn
    // makes send return -1 instead
    std::signal(SIGPIPE, SIG_IGN);

    // create socket returns file descriptor
    // IPv4, TCP
    Socket server(socket(AF_INET, SOCK_STREAM, 0));

    if (server.fd() < 0) {
        perror("socket");
        return false;
    }

    int yes = 1;
    // restart without "address in use"
    setsockopt(server.fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr {};
    // IPv4
    addr.sin_family = AF_INET;
    // host to network (big endian -> little endian, byte reversal), short (16 bits): convert port to network byte order
    addr.sin_port = htons(port);
    // host to network, long (32 bits): 127.0.0.1
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // attach server to `addr`
    // treat IPv4 like generic kind with cast, and use sizeof to know the type instead
    // i.e. IPv4 is 16. bytes IPv6 is 28
    if (bind(server.fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        return false;
    }
    // start accepting connections
    // 16 = max # of connections the OS will hold in queue
    if (listen(server.fd(), 16) < 0) {
        perror("listen");
        return false;
    }

    while (true) {
        int fd = accept(server.fd(), nullptr, nullptr);
        if (fd == -1) {
            perror("accept");
            continue;
        }
        Socket client(fd);
        handleClient(client.fd(), store, commands);
    }
    return true;
}