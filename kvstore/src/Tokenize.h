#pragma once
#include <optional>
#include <string>
#include <vector>

using Args = std::vector<std::string>;

std::optional<long long> parseInt(const std::string& s);
Args tokenize(const std::string& line);
