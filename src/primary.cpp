#include "alerts/model.hpp"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace alerts {
namespace {
std::string canonical(std::string slug, const std::vector<Location>& locations) {
    const auto exists = [&](const std::string& candidate) {
        return std::any_of(locations.begin(), locations.end(),
                           [&](const Location& l) { return l.slug == candidate; });
    };
    if (exists(slug)) return slug;
    constexpr std::string_view suffix = "-oblast";
    if (slug.size() >= suffix.size() &&
        slug.compare(slug.size() - suffix.size(), suffix.size(), suffix) == 0) {
        const auto short_slug = slug.substr(0, slug.size() - suffix.size());
        if (exists(short_slug)) return short_slug;
    } else if (exists(slug + std::string(suffix))) {
        return slug + std::string(suffix);
    }
    return slug;
}
}
DirectoryResult parse_directory(std::string_view text) {
    const auto doc = json::parse(text);
    const auto* regions = json::field(doc.get(), "regions");
    if (!doc || !cJSON_IsArray(regions) || cJSON_GetArraySize(regions) > 512)
        return {{}, "Invalid region directory"};
    DirectoryResult result;
    std::set<std::string> slugs;
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, regions) {
        Location location{json::text(item, "slug"), json::text(item, "oblast_slug"),
                          json::text(item, "name_uk")};
        if (const auto* parent = json::field(item, "parent_slug"); cJSON_IsString(parent))
            location.parent = json::text(parent);
        location.reserve_id = json::text(item, "reserve_id");
        if (location.slug.empty() || location.name.empty() || !slugs.insert(location.slug).second)
            return {{}, "Missing or duplicate region slug"};
        result.locations.push_back(std::move(location));
    }
    if (result.locations.empty()) return {{}, "Empty region directory"};
    for (auto& location : result.locations)
        location.parent = canonical(location.parent, result.locations);
    return result;
}
ParseResult parse_primary(std::string_view text, const std::vector<Location>& directory) {
    const auto doc = json::parse(text);
    const auto* records = json::field(doc.get(), "alerts");
    const auto* active = json::field(doc.get(), "active");
    const auto updated = json::text(doc.get(), "updated_at");
    if (!doc || !cJSON_IsArray(records) || !cJSON_IsNumber(active) ||
        !json::timestamp(updated) || cJSON_GetArraySize(records) > 1024 ||
        active->valuedouble != static_cast<double>(cJSON_GetArraySize(records)))
        return {{}, "Invalid primary snapshot"};
    Snapshot snapshot;
    snapshot.source_updated_at = updated;
    snapshot.available = true;
    for (const auto& location : directory) snapshot.supported.push_back(location.slug);
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, records) {
        Event event;
        event.slug = canonical(json::text(item, "slug"), directory);
        event.parent = canonical(json::text(item, "oblast_slug"), directory);
        const auto location = std::find_if(directory.begin(), directory.end(),
            [&](const Location& l) { return l.slug == event.slug; });
        if (location != directory.end()) event.parent = location->parent;
        const auto level = json::text(item, "level");
        event.level = level == "red" ? Level::red : level == "yellow" ? Level::yellow : Level::unknown;
        event.started_at = json::text(item, "started_at");
        if (event.slug.empty() || (!event.started_at.empty() && !json::timestamp(event.started_at)))
            return {{}, "Invalid alert record"};
        snapshot.events.push_back(std::move(event));
    }
    return {std::move(snapshot), ""};
}
}  // namespace alerts
