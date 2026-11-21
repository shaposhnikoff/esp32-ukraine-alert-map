#include "alerts/model.hpp"
#include "alerts/time.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace alerts;
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
const std::vector<Location> directory{
    {"kyiv", "", "Київ"},
    {"kyivska", "", "Київська область"},
    {"brovarskii-raion", "kyivska", "Броварський район"},
    {"bucanskii-raion", "kyivska", "Бучанський район"}};
ParseResult primary(const char* text) { return parse_primary(text, directory); }
}

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> cases{
        {"UTC dates work without platform timegm", [] {
            require(parse_utc("1970-01-01T00:00:00Z") == std::time_t(0), "UTC epoch is wrong");
            require(parse_utc("2024-02-29T12:30:00.123Z") == std::time_t(1709209800), "leap timestamp is wrong");
            require(!parse_utc("2023-02-29T00:00:00Z"), "invalid leap day accepted");
            require(!parse_utc("2026-11-21T25:00:00Z"), "invalid hour accepted");
        }},
        {"city remains independent from oblast", [] {
            const auto result = primary(R"({"updated_at":"2026-11-21T13:00:00Z","active":1,"alerts":[{"slug":"kyivska","level":"red","started_at":"2026-11-21T12:00:00Z"}]})");
            require(bool(result), "valid primary response rejected");
            require(status_for(result.snapshot, directory[0]).level == Level::none, "oblast alert leaked into Kyiv city");
        }},
        {"oblast alert covers its districts", [] {
            const auto result = primary(R"({"updated_at":"2026-11-21T13:00:00Z","active":1,"alerts":[{"slug":"kyivska","level":"red","started_at":"2026-11-21T12:00:00Z"}]})");
            require(status_for(result.snapshot, directory[2]).level == Level::red, "district missed inherited red alert");
        }},
        {"district alert does not color its neighbor", [] {
            const auto result = primary(R"({"updated_at":"2026-11-21T13:00:00Z","active":1,"alerts":[{"slug":"brovarskii-raion","oblast_slug":"kyivska","level":"yellow","started_at":"2026-11-21T12:00:00Z"}]})");
            require(status_for(result.snapshot, directory[2]).level == Level::yellow, "district lost yellow alert");
            require(status_for(result.snapshot, directory[3]).level == Level::none, "district alert leaked into neighbor");
            require(status_for(result.snapshot, directory[1]).partial, "oblast summary lost partial coverage");
        }},
        {"missing directory coverage stays unknown", [] {
            const auto result = primary(R"({"updated_at":"2026-11-21T13:00:00Z","active":0,"alerts":[]})");
            require(status_for(result.snapshot, {"unsupported", "", "Unknown"}).level == Level::unknown, "unsupported location became green");
        }},
        {"unknown active level cannot become green", [] {
            const auto result = primary(R"({"updated_at":"2026-11-21T13:00:00Z","active":1,"alerts":[{"slug":"kyiv","level":"future","started_at":null}]})");
            require(status_for(result.snapshot, directory[0]).level == Level::unknown, "unknown active level became inactive");
        }},
        {"unknown active record cannot be hidden by yellow", [] {
            const auto result = primary(R"({"updated_at":"2026-11-21T13:00:00Z","active":2,"alerts":[{"slug":"kyiv","level":"future","started_at":null},{"slug":"kyiv","level":"yellow","started_at":null}]})");
            require(status_for(result.snapshot, directory[0]).level == Level::unknown, "yellow concealed an unclassified active alert");
        }},
        {"community parent metadata overrides oblast grouping", [] {
            const auto catalog = parse_directory(R"({"regions":[{"slug":"kyivska","name_uk":"Київська область"},{"slug":"brovarskii-raion","name_uk":"Броварський район","oblast_slug":"kyivska"},{"slug":"community","name_uk":"Громада","oblast_slug":"kyivska","parent_slug":"brovarskii-raion","reserve_id":"711"}]})");
            const auto result = parse_primary(R"({"updated_at":"2026-11-21T13:00:00Z","active":1,"alerts":[{"slug":"community","oblast_slug":"kyivska","level":"red","started_at":null}]})", catalog.locations);
            const auto status = status_for(result.snapshot, directory[2]);
            require(status.level == Level::red && status.partial, "community did not mark its actual district partially red");
        }},
        {"bad count and boolean count rejected", [] {
            require(!primary(R"({"updated_at":"2026-11-21T13:00:00Z","active":true,"alerts":[]})"), "boolean count accepted");
            require(!primary(R"({"updated_at":"2026-11-21T13:00:00Z","active":2,"alerts":[]})"), "inconsistent count accepted");
        }},
        {"malformed and missing fields rejected", [] {
            require(!primary("{"), "malformed JSON accepted");
            require(!primary(R"({"active":0,"alerts":[]})"), "missing source time accepted");
            require(!primary(R"({"updated_at":"2026-11-21T13:00:00Z","active":1,"alerts":[{}]})"), "missing slug accepted");
        }},
        {"reserve filters non AIR and preserves yellow", [] {
            const auto result = parse_reserve(R"([{"regionId":"31","regionName":"Київ","activeAlerts":[{"type":"ARTILLERY","activeAlertLevels":[{"alertLevel":"Red"}]},{"type":"AIR","activeAlertLevels":[{"alertLevel":"Yellow","createdAt":"2026-11-21T12:00:00Z"}]}]}])", directory);
            require(bool(result), "reserve response rejected");
            require(status_for(result.snapshot, directory[0]).level == Level::yellow, "reserve included non-AIR or lost yellow");
        }},
        {"reserve omission remains unknown", [] {
            const auto result = parse_reserve("[]", directory);
            require(status_for(result.snapshot, directory[0]).level == Level::unknown, "unverified reserve omission became all-clear");
        }},
        {"freshness uses monotonic validation time", [] {
            Snapshot s; s.available = true; s.validated_ms = 1000;
            require(is_fresh(s, 180999, 180000), "fresh boundary marked stale");
            require(!is_fresh(s, 181000, 180000), "stale boundary marked fresh");
            require(!is_fresh(Snapshot{}, 0, 180000), "startup marked fresh");
        }},
        {"buzzer signals each newly affected Kyiv zone once", [] {
            KyivAlertSignal signal(directory);
            Snapshot update;
            update.available = true;
            update.supported = {"kyiv", "brovarskii-raion", "bucanskii-raion"};
            update.events = {{"kyiv", "", Level::yellow, ""}};
            require(!signal.update(update), "existing alert sounded at startup");
            update.events[0].level = Level::red;
            update.events.push_back({"lvivska", "", Level::red, ""});
            require(!signal.update(update), "severity change or outside alert triggered buzzer");
            update.events.push_back({"brovarskii-raion", "kyivska", Level::red, ""});
            require(signal.update(update), "new district alert while Kyiv was active was missed");
            require(!signal.update(update), "same alert sounded twice");
            update.events.clear();
            require(!signal.update(update), "all-clear triggered buzzer");
            update.events.push_back({"kyivska", "", Level::red, ""});
            require(signal.update(update), "new oblast-wide alert was missed");
            require(!signal.update(update), "second district in same update caused a repeat");
            update.events.push_back({"kyiv", "", Level::red, ""});
            require(signal.update(update), "new city alert while oblast was active was missed");
        }},
        {"buzzer retains zone state through unknown and demo updates", [] {
            KyivAlertSignal signal(directory);
            Snapshot update;
            update.available = true;
            update.supported = {"kyiv", "brovarskii-raion"};
            require(!signal.update(update), "initial all-clear triggered buzzer");
            update.provider = Provider::demo;
            update.events = {{"kyiv", "", Level::red, ""}};
            require(!signal.update(update), "demo triggered buzzer");
            update.provider = Provider::primary;
            require(signal.update(update), "demo altered the live baseline");
            Snapshot omitted;
            omitted.available = true;
            omitted.provider = Provider::reserve;
            require(!signal.update(omitted), "reserve omission triggered buzzer");
            require(!signal.update(Snapshot{}), "missing snapshot triggered buzzer");
            require(!signal.update(update), "unknown status rearmed the active city alert");
            update.events.push_back({"brovarskii-raion", "kyivska", Level::yellow, ""});
            require(signal.update(update), "unknown status erased a previously clear district");
        }},
        {"directory resolves parent aliases", [] {
            const auto result = parse_directory(R"({"regions":[{"slug":"kyivska","type":"state","name_uk":"Київська область"},{"slug":"brovarskii-raion","type":"district","name_uk":"Броварський район","oblast_slug":"kyivska-oblast"}]})");
            require(bool(result), "valid directory rejected");
            require(result.locations[1].parent == "kyivska", "parent alias unresolved");
        }}
    };
    int failures = 0;
    for (const auto& [name, test] : cases) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    }
    return failures ? 1 : 0;
}
