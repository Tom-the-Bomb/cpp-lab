#pragma once
#include <string>
#include <vector>
#include <optional>

using Args = std::vector<std::string>;

std::optional<long long> parseInt(const std::string& s);
Args tokenize(const std::string& line);