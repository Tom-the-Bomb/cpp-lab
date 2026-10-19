#include "Resp.h"

#include <cstddef>
#include <string>
#include <utility>

#include "Tokenize.h"

constexpr std::size_t maxNumEl = 1e6;
constexpr std::size_t maxArgSize = 1e6;

ParseResult parseResp(const std::string& buf) {
    if (buf[0] != '*') {
        return ParseResult {ParseStatus::Invalid};
    }
    std::size_t i = 1;
    std::size_t n = buf.size();
    i = buf.find("\r\n", i);
    if (i == buf.npos) {
        return ParseResult {ParseStatus::Incomplete};
    }
    auto cnt = parseInt<std::size_t>(buf.substr(1, i - 1));
    if (!cnt || cnt > maxNumEl) {
        return ParseResult {ParseStatus::Invalid};
    }
    i += 2;
    Args args;
    for (std::size_t k = 0; k < *cnt; k++) {
        if (i == n) {
            return ParseResult {ParseStatus::Incomplete};
        }
        if (buf[i] != '$') {
            return ParseResult {ParseStatus::Invalid};
        }
        std::size_t nextI = buf.find("\r\n", i);
        if (nextI == buf.npos) {
            return ParseResult {ParseStatus::Incomplete};
        }
        auto len = parseInt<std::size_t>(buf.substr(i + 1, nextI - i - 1));
        if (!len || *len > maxArgSize) {
            return ParseResult {ParseStatus::Invalid};
        }
        i = nextI + 2;
        if (n < i + *len + 2) {
            return ParseResult {ParseStatus::Incomplete};
        }
        if (buf.substr(i + *len, 2) != "\r\n") {
            return ParseResult {ParseStatus::Invalid};
        }
        args.push_back(buf.substr(i, *len));
        i += *len + 2;
    }
    return ParseResult {ParseStatus::Complete, std::move(args), i};
}
