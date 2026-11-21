#include "alerts/time.hpp"
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <string>

namespace alerts {
std::optional<std::time_t> parse_utc(std::string_view value) {
    if (value.size() < 20 || value.size() > 32 || value[4] != '-' ||
        value[7] != '-' || value[10] != 'T' || value[13] != ':' ||
        value[16] != ':' || value.back() != 'Z') return std::nullopt;
    for (size_t i = 0; i < 19; ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) return std::nullopt;
    }
    if (value.size() > 20) {
        if (value[19] != '.' || value.size() == 21) return std::nullopt;
        for (size_t i = 20; i + 1 < value.size(); ++i)
            if (!std::isdigit(static_cast<unsigned char>(value[i]))) return std::nullopt;
    }
    int year, month, day, hour, minute, second;
    if (std::sscanf(std::string(value).c_str(), "%d-%d-%dT%d:%d:%d",
                    &year, &month, &day, &hour, &minute, &second) != 6) return std::nullopt;
    if (year < 1970 || year > 2200 || month < 1 || month > 12 ||
        day < 1 || hour > 23 || minute > 59 || second > 59) return std::nullopt;
    constexpr int days_per_month[]{31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int maximum = days_per_month[month - 1] + ((month == 2 && leap) ? 1 : 0);
    if (day > maximum) return std::nullopt;
    const int adjusted_year = year - (month <= 2 ? 1 : 0);
    const int era = adjusted_year / 400;
    const int yoe = adjusted_year - era * 400;
    const int shifted_month = month + (month > 2 ? -3 : 9);
    const int doy = (153 * shifted_month + 2) / 5 + day - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t days = int64_t(era) * 146097 + doe - 719468;
    return static_cast<std::time_t>(days * 86400 + hour * 3600 + minute * 60 + second);
}
}
