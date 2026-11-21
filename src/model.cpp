#include "alerts/model.hpp"
#include <algorithm>

namespace alerts {
namespace {
int priority(Level level) {
    switch (level) {
        case Level::none: return 0;
        case Level::yellow: return 1;
        case Level::unknown: return 2;
        case Level::red: return 3;
    }
    return 2;
}
}
Status status_for(const Snapshot& snapshot, const Location& location) {
    if (!snapshot.available) return {};
    const bool supported = std::find(snapshot.supported.begin(), snapshot.supported.end(),
                                     location.slug) != snapshot.supported.end();
    Status result{Level::none, false, ""};
    bool confirmed = supported;
    for (const auto& event : snapshot.events) {
        const bool direct = event.slug == location.slug;
        const bool inherited = !location.parent.empty() && event.slug == location.parent;
        const bool subordinate = event.parent == location.slug;
        if (!(direct || inherited || subordinate)) continue;
        confirmed = true;
        if (priority(event.level) > priority(result.level)) {
            result.level = event.level;
            result.started_at = event.started_at;
            result.partial = subordinate && !(direct || inherited);
        } else if (priority(event.level) == priority(result.level) && (direct || inherited)) {
            result.partial = false;
        }
    }
    if (!confirmed) result.level = Level::unknown;
    return result;
}
bool is_fresh(const Snapshot& snapshot, uint64_t now_ms, uint64_t stale_ms) {
    return snapshot.available && now_ms >= snapshot.validated_ms &&
           now_ms - snapshot.validated_ms < stale_ms;
}

KyivAlertSignal::KyivAlertSignal(const std::vector<Location>& directory) {
    for (const auto& location : directory) {
        if (location.slug == "kyiv" || location.parent == "kyivska")
            zones_.push_back({location});
    }
}

bool KyivAlertSignal::update(const Snapshot& snapshot) {
    if (!snapshot.available || snapshot.provider == Provider::demo) return false;

    bool beep = false;
    for (auto& zone : zones_) {
        const auto level = status_for(snapshot, zone.location).level;
        if (level == Level::unknown) continue;
        if (zone.previous == Level::none && level != Level::none) beep = true;
        zone.previous = level;
    }
    return beep;
}
}  // namespace alerts
