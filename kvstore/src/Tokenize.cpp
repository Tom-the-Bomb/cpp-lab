#include "Tokenize.h"

#include <cctype>
#include <charconv>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

constexpr char hexDigits[] = "0123456789abcdef";

static bool isQuote(char c) {
    return c == '"' || c == '\'';
}

static std::optional<int> toHex(char c) {
    int d;
    if ('a' <= c && c <= 'f') {
        d = c - 'a' + 10;
    } else if ('A' <= c && c <= 'F') {
        d = c - 'A' + 10;
    } else if ('0' <= c && c <= '9') {
        d = c - '0';
    } else {
        return std::nullopt;
    }
    return d;
}

static std::optional<char> toEscape(char c) {
    switch (c) {
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case 'b':
            return '\b';
        case 'a':
            return '\a';
        case '\\':
            return '\\';
        default:
            return std::nullopt;
    }
}

// stoll() but according to Redis behavior
std::optional<long long> parseInt(const std::string& s) {
    long long n;
    // ptr to 1 past the last character
    const char* end = s.data() + s.size();
    // `ptr` is where it stopped, `s` being int means `ptr` should be 1 past the last character
    // otherwise something like `10abc` was parsed
    // `ec` is error code, `errc` is success
    auto [ptr, ec] = std::from_chars(s.data(), end, n);
    if (ec != std::errc {} || ptr != end) {
        return std::nullopt;
    }
    return n;
}

Args tokenize(const std::string& line) {
    Args tokens;

    std::size_t i = 0, n = line.length();
    std::string token;
    // don't simply push on every whitespace encountered, but also push empty
    bool tokenEmpty = true;

    while (i < n) {
        bool isSpace = std::isspace(static_cast<unsigned char>(line[i]));

        if (i > 0 && isQuote(line[i - 1]) && !isSpace) {
            // match Redis behavior
            throw std::invalid_argument("Quotations must be immediately followed by whitespace");
        }

        if (isQuote(line[i])) {
            std::size_t j = i + 1;
            while (true) {
                if (j == n) {
                    throw std::invalid_argument("Unbalanced quotes in request");
                }
                if (line[j] == '\\') {
                    // \' in '' or \" in "" should append the quote
                    if (j + 1 < n && line[j + 1] == line[i]) {
                        token.push_back(line[i]);
                        // j++ to skip \, the later global j++ will skip '/"
                        j++;
                        // escape sequences only work in "" (Redis)
                    } else if (line[i] == '"') {
                        if (j + 3 < n && line[j + 1] == 'x') {
                            auto d1 = toHex(line[j + 2]);
                            auto d2 = toHex(line[j + 3]);
                            // successfully encountered \x[0-9a-fA-F][0-9a-fA-F]
                            if (d1 && d2) {
                                token.push_back(static_cast<char>(*d1 * 16 + *d2));
                                // skip and done esc seq processing
                                // later j++ will skip last char
                                j += 3;
                            }
                        } else if (j + 1 < n) {
                            auto c = toEscape(line[j + 1]);
                            // successfully encountered valid escape sequence:
                            // \n \t ... \\ etc.
                            if (c) {
                                token.push_back(*c);
                                // j++ to skip \, the later global j++ will skip the char
                                j++;
                            }
                        }
                    } else if (line[i] == '\'') {
                        // later global j++ will skip \, we don't want that
                        // so add it first
                        token.push_back('\\');
                    }
                } else if (line[j] == line[i]) {
                    // found matching quotation
                    break;
                } else {
                    // normal tokens
                    token.push_back(line[j]);
                }
                j++;
            }
            tokenEmpty = false;
            // skip past end quote
            i = j + 1;
            continue;
        }

        if (isSpace && !tokenEmpty) {
            tokens.push_back(token);
            token.clear();
            tokenEmpty = true;
        } else if (!isSpace) {
            token.push_back(line[i]);
            tokenEmpty = false;
        }
        i++;
    }
    if (!tokenEmpty) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '\\':
                out.push_back('\\');
                out.push_back('\\');
                break;
            case '"':
                out.push_back('\\');
                out.push_back('"');
                break;
            case '\n':
                out.push_back('\\');
                out.push_back('n');
                break;
            case '\r':
                out.push_back('\\');
                out.push_back('r');
                break;
            case '\t':
                out.push_back('\\');
                out.push_back('t');
                break;
            default: {
                // (0-31) 0x00-0x1F and (127) 0x7F are control characters (\a \b etc.)
                if (c < 0x20 || c == 0x7F) {
                    out.push_back('\\');
                    out.push_back('x');
                    // (c) a byte (8 bits) is 2 hex digits (4 bits)
                    // c >> 4 gets the first 4 bits (0-15) -> conv to corresponding char
                    out.push_back(hexDigits[c >> 4]);
                    // c & 0xF (1111) gets the last 4 bits
                    out.push_back(hexDigits[c & 0xF]);
                } else {
                    out.push_back(c);
                }
            }
        }
    }
    out.push_back('"');
    return out;
}
