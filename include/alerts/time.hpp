#pragma once
#include <ctime>
#include <optional>
#include <string_view>

namespace alerts {
std::optional<std::time_t> parse_utc(std::string_view value);
}
