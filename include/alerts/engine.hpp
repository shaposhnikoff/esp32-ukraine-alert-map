#pragma once
#include "alerts/ui.hpp"
#include <deque>
#include <optional>

namespace alerts {
struct Request { Provider provider; std::string url; };
struct HttpResult {
    int status{};
    std::string body;
    std::string error;
    uint64_t retry_ms{};
};
struct PollConfig {
    std::string primary_url;
    std::string reserve_url;
    uint64_t poll_ms{60000};
    uint64_t stale_ms{180000};
};
class Engine {
public:
    Engine(PollConfig config, std::vector<Location> directory);
    std::optional<Request> request(uint64_t now_ms);
    void complete(const Request& request, HttpResult result, uint64_t now_ms);
    UiState state(uint64_t now_ms) const;
private:
    PollConfig config_;
    std::vector<Location> directory_;
    UiState state_;
    std::deque<uint64_t> attempts_;
    uint64_t due_{};
    uint64_t primary_probe_{};
    unsigned failures_{};
    unsigned recoveries_{};
    bool reserve_{};
    bool in_flight_{};
};
Snapshot demo_snapshot(const std::vector<Location>& directory);
}  // namespace alerts
