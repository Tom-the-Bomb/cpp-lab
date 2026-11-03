#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Fd.h"

inline constexpr std::size_t pageSize = 4096;

class Pager {
public:
    class Page;

    explicit Pager(const std::string& path);
    ~Pager();

    // pager owns the open file (fd)
    // which cannot be copied as if it is destroyed it closes the file descriptor and affects the copy
    Pager(const Pager&) = delete;
    Pager& operator=(const Pager&) = delete;

    Page getPage(std::uint32_t n);
    std::uint32_t newPage();
    std::uint32_t numPages() const;

    void flush();
private:
    struct Slot {
        std::unique_ptr<std::byte[]> bytes;
        // has been written to, need to write back
        bool dirty = false;
        // reference counting
        std::size_t pins = 0;
    };

    Fd fd_;
    std::uint32_t numPages_ = 0;
    std::vector<Slot> cache_;
};

class Pager::Page {
public:
    const std::byte* data() const;
    std::byte* mutableData();
    std::uint32_t number() const;

    ~Page();
    Page(Page&& other) noexcept;
    Page& operator=(Page&& other) noexcept;
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
private:
    friend class Pager;
    Page(Pager* pager, std::uint32_t n);

    Pager* pager_;
    std::uint32_t n_;
};
