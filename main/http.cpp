#include "platform.hpp"
#include "alerts_config.hpp"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <strings.h>

namespace {
esp_err_t header(esp_http_client_event_t* event) {
    auto* result = static_cast<alerts::HttpResult*>(event->user_data);
    if (event->event_id == HTTP_EVENT_ON_HEADER &&
        strcasecmp(event->header_key, "Retry-After") == 0) {
        char* end = nullptr;
        const auto seconds = std::strtoull(event->header_value, &end, 10);
        if (end != event->header_value && *end == '\0' && seconds <= 86400)
            result->retry_ms = seconds * 1000;
    }
    return ESP_OK;
}
}
alerts::HttpResult alerts_http_get(const std::string& url) {
    alerts::HttpResult result;
    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.timeout_ms = ALERTS_TIMEOUT_MS;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.user_agent = "UkraineAlerts/0.1 (ESP32-S3 7B)";
    config.event_handler = header;
    config.user_data = &result;
    config.buffer_size = 2048;
    using Client = std::unique_ptr<esp_http_client, decltype(&esp_http_client_cleanup)>;
    Client client(esp_http_client_init(&config), esp_http_client_cleanup);
    if (!client) return {0, "", "HTTPS client unavailable", 0};
    const auto opened = esp_http_client_open(client.get(), 0);
    if (opened != ESP_OK) return {0, "", esp_err_to_name(opened), 0};
    const auto length = esp_http_client_fetch_headers(client.get());
    result.status = esp_http_client_get_status_code(client.get());
    if (length > 262144) return {result.status, "", "Response exceeds 256 KiB", 0};
    char buffer[2048];
    while (true) {
        const int received = esp_http_client_read(client.get(), buffer, sizeof(buffer));
        if (received < 0) return {result.status, "", "HTTPS read failed", result.retry_ms};
        if (received == 0) break;
        if (result.body.size() + static_cast<size_t>(received) > 262144)
            return {result.status, "", "Response exceeds 256 KiB", result.retry_ms};
        result.body.append(buffer, static_cast<size_t>(received));
    }
    if (!esp_http_client_is_complete_data_received(client.get()))
        result.error = "Incomplete HTTPS response";
    return result;
}
