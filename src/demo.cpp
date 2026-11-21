#include "alerts/engine.hpp"

namespace alerts {
Snapshot demo_snapshot(const std::vector<Location>& directory) {
    Snapshot snapshot;
    snapshot.provider = Provider::demo;
    snapshot.available = true;
    for (const auto& location : directory) snapshot.supported.push_back(location.slug);
    snapshot.events = {
        {"brovarskii-raion", "kyivska", Level::yellow, "2026-11-21T12:08:00Z"},
        {"borispilskii-raion", "kyivska", Level::yellow, "2026-11-21T12:10:00Z"},
        {"obuxivskii-raion", "kyivska", Level::red, "2026-11-21T12:15:00Z"}
    };
    return snapshot;
}
}  // namespace alerts
