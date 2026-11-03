#include "Pager.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "Fd.h"

static std::runtime_error sysError(const std::string& what) {
    return std::runtime_error(what + ": " + strerror(errno));
}

Pager::Pager(const std::string& path) {
    fd_ = Fd(open(path.c_str(), O_RDWR | O_CREAT, 0644));
    if (!fd_.isOpen()) {
        throw sysError(path + " cannot be opened");
    }

    struct stat st;
    if (fstat(fd_.fd(), &st) < 0) {
        throw sysError("File size could not be determined");
    }
    if (st.st_size % pageSize != 0) {
        throw std::runtime_error("File has corrupt page");
    }
    numPages_ = st.st_size / pageSize;
    cache_.resize(numPages_);
}

Pager::~Pager() {
    try {
        flush();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

std::uint32_t Pager::newPage() {
    cache_.push_back(Slot {std::make_unique<std::byte[]>(pageSize), true});
    std::uint32_t oldNumPages = numPages_;
    numPages_++;
    return oldNumPages;
}

Pager::Page Pager::getPage(std::uint32_t n) {
    if (n >= numPages_) {
        throw std::runtime_error("page number out of range");
    }
    // cache miss
    if (!cache_[n].bytes) {
        std::unique_ptr<std::byte[]> buffer = std::make_unique_for_overwrite<std::byte[]>(pageSize);
        ssize_t read = pread(fd_.fd(), buffer.get(), pageSize, n * pageSize);

        if (read != pageSize) {
            throw read < 0 ? sysError("Read failed") : std::runtime_error("Short file");
        }
        cache_[n].bytes = std::move(buffer);
    }
    return Page(this, n);
}

std::uint32_t Pager::numPages() const {
    return numPages_;
}

void Pager::flush() {
    for (std::size_t n = 0; n < numPages_; n++) {
        Slot& slot = cache_[n];
        if (slot.bytes && slot.dirty) {
            ssize_t written = pwrite(fd_.fd(), slot.bytes.get(), pageSize, n * pageSize);
            if (written != pageSize) {
                throw written < 0 ? sysError("Write failed") : std::runtime_error("Short write");
            }
            slot.dirty = false;
        }
    }
    if (!syncFd(fd_.fd())) {
        throw sysError("Failed to fsync to disk");
    }
}

Pager::Page::Page(Pager* pager, std::uint32_t n): pager_(pager), n_(n) {
    pager_->cache_[n_].pins++;
}

Pager::Page::~Page() {
    if (pager_) {
        pager_->cache_[n_].pins--;
    }
}

const std::byte* Pager::Page::data() const {
    return pager_->cache_[n_].bytes.get();
}

std::byte* Pager::Page::mutableData() {
    pager_->cache_[n_].dirty = true;
    return pager_->cache_[n_].bytes.get();
}

std::uint32_t Pager::Page::number() const {
    return n_;
}

Pager::Page::Page(Page&& other) noexcept: pager_(other.pager_), n_(other.n_) {
    other.pager_ = nullptr;
}

Pager::Page& Pager::Page::operator=(Page&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    if (pager_) {
        pager_->cache_[n_].pins--;
    }
    pager_ = other.pager_;
    n_ = other.n_;
    other.pager_ = nullptr;
    return *this;
}
