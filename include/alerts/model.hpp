#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace alerts {
enum class Level : uint8_t { none, yellow, red, unknown };
enum class Provider : uint8_t { primary, reserve, demo };
struct Location {
    std::string slug;
    std::string parent;
    std::string name;
    std::string reserve_id{};
};
struct Event {
    std::string slug;
    std::string parent;
    Level level{Level::unknown};
    std::string started_at;
};
struct Snapshot {
    Provider provider{Provider::primary};
    std::vector<Event> events;
    std::vector<std::string> supported;
    std::string source_updated_at;
    uint64_t validated_ms{};
    bool available{};
};
struct Status {
    Level level{Level::unknown};
    bool partial{};
    std::string started_at;
};
struct ParseResult {
    Snapshot snapshot;
    std::string error;
    explicit operator bool() const { return error.empty(); }
};
struct DirectoryResult {
    std::vector<Location> locations;
    std::string error;
    explicit operator bool() const { return error.empty(); }
};
DirectoryResult parse_directory(std::string_view json);
ParseResult parse_primary(std::string_view json, const std::vector<Location>& directory);
ParseResult parse_reserve(std::string_view json, const std::vector<Location>& directory);
Status status_for(const Snapshot& snapshot, const Location& location);
bool is_fresh(const Snapshot& snapshot, uint64_t now_ms, uint64_t stale_ms);

class KyivAlertSignal {
public:
    explicit KyivAlertSignal(const std::vector<Location>& directory);
    bool update(const Snapshot& snapshot);

private:
    struct Zone {
        Location location;
        Level previous = Level::unknown;
    };
    std::vector<Zone> zones_;
};
}  // namespace alerts
