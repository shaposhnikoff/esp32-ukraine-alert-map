#include "alerts/engine.hpp"
#include <algorithm>
#include <utility>

namespace alerts {
Engine::Engine(PollConfig config, std::vector<Location> directory)
    : config_(std::move(config)), directory_(std::move(directory)) {
    state_.stale_ms = config_.stale_ms;
}
std::optional<Request> Engine::request(uint64_t now_ms) {
    while (!attempts_.empty() && now_ms - attempts_.front() >= 60000) attempts_.pop_front();
    if (in_flight_ || now_ms < due_ || attempts_.size() >= 4) return std::nullopt;
    const bool probe = reserve_ && now_ms >= primary_probe_;
    const auto provider = reserve_ && !probe ? Provider::reserve : Provider::primary;
    in_flight_ = true;
    attempts_.push_back(now_ms);
    due_ = now_ms + config_.poll_ms;
    return Request{provider, provider == Provider::primary ? config_.primary_url : config_.reserve_url};
}
void Engine::complete(const Request& request, HttpResult result, uint64_t now_ms) {
    in_flight_ = false;
    ParseResult parsed;
    if (result.status == 200 && result.error.empty())
        parsed = request.provider == Provider::primary
            ? parse_primary(result.body, directory_) : parse_reserve(result.body, directory_);
    else parsed.error = result.error.empty() ? "HTTP " + std::to_string(result.status) : result.error;
    if (parsed) {
        if (request.provider == Provider::primary && reserve_ && ++recoveries_ < 2) {
            primary_probe_ = now_ms + 300000;
            state_.connection = "Резерв · основне API відновлюється";
            return;
        }
        if (request.provider == Provider::primary) {
            reserve_ = false;
            failures_ = recoveries_ = 0;
        }
        parsed.snapshot.validated_ms = now_ms;
        state_.snapshot = std::move(parsed.snapshot);
        state_.connection = request.provider == Provider::reserve ? "UA Siren · резерв" : "Wi-Fi · дані отримано";
        return;
    }
    state_.connection = "Дані недоступні · " + parsed.error;
    if (result.status == 429) {
        due_ = now_ms + std::max(result.retry_ms, config_.poll_ms);
        return;
    }
    if (request.provider == Provider::primary) {
        recoveries_ = 0;
        if (reserve_) primary_probe_ = now_ms + 300000;
        else if (++failures_ >= 2) {
            reserve_ = true;
            primary_probe_ = now_ms + 300000;
            due_ = now_ms + 1000;
        }
    }
}
UiState Engine::state(uint64_t now_ms) const {
    auto state = state_;
    state.now_ms = now_ms;
    return state;
}
}  // namespace alerts
