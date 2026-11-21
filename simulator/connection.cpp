#include "connection.hpp"
#include "alerts/engine.hpp"
#include "alerts/geometry.hpp"
#include "alerts_config.hpp"
#include <SDL.h>
#include <curl/curl.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <strings.h>

namespace alerts {
namespace {
size_t body(char* data, size_t size, size_t count, void* context) {
    auto* result = static_cast<HttpResult*>(context);
    const auto length = size * count;
    if (length > 262144 - result->body.size()) return 0;
    result->body.append(data, length);
    return length;
}
size_t header(char* data, size_t size, size_t count, void* context) {
    auto* result = static_cast<HttpResult*>(context);
    const std::string line(data, size * count);
    if (line.size() > 12 && strncasecmp(line.c_str(), "Retry-After:", 12) == 0) {
        char* end = nullptr;
        const auto seconds = std::strtoull(line.c_str() + 12, &end, 10);
        if (end != line.c_str() + 12 && seconds <= 86400) result->retry_ms = seconds * 1000;
    }
    return size * count;
}
HttpResult get(const std::string& url) {
    HttpResult result;
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> client(curl_easy_init(), curl_easy_cleanup);
    if (!client) return {0, "", "HTTPS client unavailable", 0};
    curl_easy_setopt(client.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(client.get(), CURLOPT_TIMEOUT_MS, long(ALERTS_TIMEOUT_MS));
    curl_easy_setopt(client.get(), CURLOPT_CONNECTTIMEOUT_MS, long(ALERTS_TIMEOUT_MS));
    curl_easy_setopt(client.get(), CURLOPT_USERAGENT, "UkraineAlerts/0.1 (desktop simulator)");
    curl_easy_setopt(client.get(), CURLOPT_WRITEFUNCTION, body);
    curl_easy_setopt(client.get(), CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(client.get(), CURLOPT_HEADERFUNCTION, header);
    curl_easy_setopt(client.get(), CURLOPT_HEADERDATA, &result);
    const auto code = curl_easy_perform(client.get());
    long status = 0;
    curl_easy_getinfo(client.get(), CURLINFO_RESPONSE_CODE, &status);
    result.status = static_cast<int>(status);
    if (code != CURLE_OK) result.error = curl_easy_strerror(code);
    return result;
}
}
Connection::Connection(StateStore& store) : store_(store) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        throw std::runtime_error("CURL initialization failed");
    thread_ = std::thread(&Connection::run, this);
}
Connection::~Connection() {
    stopping_ = true;
    signal_.notify_one();
    thread_.join();
    curl_global_cleanup();
}
void Connection::run() {
    const auto directory = parse_directory(geo::directory_json);
    if (!directory) {
        UiState state;
        state.connection = directory.error;
        store_.publish(std::move(state));
        return;
    }
    if (ALERTS_DEMO) {
        UiState state;
        state.demo = true;
        state.connection = "Демонстраційний режим";
        state.snapshot = demo_snapshot(directory.locations);
        state.snapshot.validated_ms = SDL_GetTicks64();
        state.now_ms = state.snapshot.validated_ms;
        store_.publish(std::move(state));
        return;
    }
    Engine engine({ALERTS_PRIMARY_URL, ALERTS_RESERVE_URL, ALERTS_POLL_MS, ALERTS_STALE_MS},
                  directory.locations);
    while (!stopping_) {
        if (const auto request = engine.request(SDL_GetTicks64())) {
            auto result = get(request->url);
            if (result.status != 200 || !result.error.empty())
                std::cerr << "API request failed: provider=" << int(request->provider)
                          << " status=" << result.status << " error=" << result.error << '\n';
            engine.complete(*request, std::move(result), SDL_GetTicks64());
            auto state = engine.state(SDL_GetTicks64());
            if (state.snapshot.available)
                std::cout << "LIVE provider=" << int(state.snapshot.provider)
                          << " events=" << state.snapshot.events.size() << '\n';
            store_.publish(std::move(state));
        }
        std::unique_lock<std::mutex> lock(mutex_);
        signal_.wait_for(lock, std::chrono::milliseconds(200), [&] { return stopping_.load(); });
    }
}
}
