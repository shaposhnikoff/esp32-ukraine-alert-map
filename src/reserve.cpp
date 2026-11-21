#include "alerts/model.hpp"
#include "json.hpp"
#include <algorithm>

namespace alerts {
namespace {
std::string normalized_name(std::string value) {
    const std::string prefix = "м. ";
    if (value.compare(0, prefix.size(), prefix) == 0) value.erase(0, prefix.size());
    return value;
}
}
ParseResult parse_reserve(std::string_view text, const std::vector<Location>& directory) {
    const auto doc = json::parse(text);
    if (!doc || !cJSON_IsArray(doc.get()) || cJSON_GetArraySize(doc.get()) > 1024)
        return {{}, "Invalid reserve snapshot"};
    Snapshot snapshot;
    snapshot.provider = Provider::reserve;
    snapshot.available = true;
    const cJSON* region = nullptr;
    cJSON_ArrayForEach(region, doc.get()) {
        const auto name = normalized_name(json::text(region, "regionName"));
        const auto id = json::text(region, "regionId");
        const auto location = std::find_if(directory.begin(), directory.end(),
            [&](const Location& l) {
                return !l.reserve_id.empty() ? l.reserve_id == id : normalized_name(l.name) == name;
            });
        const auto* records = json::field(region, "activeAlerts");
        if (!cJSON_IsArray(records)) return {{}, "Missing reserve alert array"};
        if (location == directory.end()) continue;
        const cJSON* item = nullptr;
        cJSON_ArrayForEach(item, records) {
            if (json::text(item, "type") != "AIR") continue;
            const auto* levels = json::field(item, "activeAlertLevels");
            if (!cJSON_IsArray(levels)) return {{}, "Missing reserve levels"};
            Event event{location->slug, location->parent, Level::unknown, ""};
            const cJSON* level = nullptr;
            cJSON_ArrayForEach(level, levels) {
                const auto label = json::text(level, "alertLevel");
                if (label == "Red") {
                    event.level = Level::red;
                    event.started_at = json::text(level, "createdAt");
                    break;
                }
                if (label == "Yellow") {
                    event.level = Level::yellow;
                    event.started_at = json::text(level, "createdAt");
                }
            }
            snapshot.supported.push_back(location->slug);
            snapshot.events.push_back(std::move(event));
        }
    }
    return {std::move(snapshot), ""};
}
}  // namespace alerts
