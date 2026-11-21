#include "alerts/engine.hpp"
#include <iostream>
#include <stdexcept>

using namespace alerts;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    try {
        const std::vector<Location> directory{{"kyiv", "", "Київ"}};
        Engine engine({"https://primary", "https://reserve", 60000, 180000}, directory);
        const auto first = engine.request(0);
        require(first && first->provider == Provider::primary, "startup must poll primary");
        require(!engine.request(1), "concurrent request allowed");
        engine.complete(*first, {200, R"({"updated_at":"2026-11-21T13:00:00Z","active":1,"alerts":[{"slug":"kyiv","level":"red","started_at":null}]})", "", 0}, 10);
        require(engine.state(10).snapshot.available, "valid snapshot not published");
        require(!engine.request(59999), "polled before interval");
        auto second = engine.request(60000);
        require(bool(second), "poll not scheduled");
        engine.complete(*second, {503, "", "", 0}, 60010);
        require(status_for(engine.state(60010).snapshot, directory[0]).level == Level::red, "failure cleared alert");
        auto third = engine.request(120000);
        require(bool(third), "second primary attempt missing");
        engine.complete(*third, {503, "", "", 0}, 120010);
        auto reserve = engine.request(121010);
        require(reserve && reserve->provider == Provider::reserve, "reserve not scheduled after two failures");
        engine.complete(*reserve, {200, "[]", "", 0}, 121020);
        require(engine.state(121020).snapshot.provider == Provider::reserve, "reserve source not published");
        require(status_for(engine.state(121020).snapshot, directory[0]).level == Level::unknown, "empty reserve became green");
        auto throttled = engine.request(181010);
        require(bool(throttled), "reserve poll missing");
        engine.complete(*throttled, {429, "", "", 300000}, 181020);
        require(!engine.request(240000), "Retry-After ignored");
        require(!is_fresh(engine.state(400000).snapshot, 400000, 180000), "stale reserve marked fresh");
        std::cout << "PASS scheduler, single flight, retained alert, reserve, Retry-After and stale state\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
