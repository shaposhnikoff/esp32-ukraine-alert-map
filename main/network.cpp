#include "platform.hpp"
#include "alerts/geometry.hpp"
#include "alerts_config.hpp"
#include "board.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <cstring>

namespace {
EventGroupHandle_t network_events;
constexpr EventBits_t connected_bit = 1;
void log_wifi_heap(const char* stage) {
    constexpr auto caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    ESP_LOGI("alerts", "Wi-Fi init %s: internal heap free=%zu bytes, largest block=%zu bytes",
             stage, heap_caps_get_free_size(caps), heap_caps_get_largest_free_block(caps));
}
void on_event(void*, esp_event_base_t base, int32_t event, void*) {
    if (base == IP_EVENT && event == IP_EVENT_STA_GOT_IP) xEventGroupSetBits(network_events, connected_bit);
    if (base == WIFI_EVENT && event == WIFI_EVENT_STA_DISCONNECTED) xEventGroupClearBits(network_events, connected_bit);
}
uint64_t now_ms() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
void worker(void* context) {
    auto* store = static_cast<alerts::StateStore*>(context);
    const auto directory = alerts::parse_directory(alerts::geo::directory_json);
    if (!directory) {
        alerts::UiState state;
        state.connection = "Помилка каталогу карти";
        store->publish(std::move(state));
        vTaskDelete(nullptr);
        return;
    }
    if (ALERTS_DEMO) {
        alerts::UiState state;
        state.demo = true;
        state.connection = "Демонстраційний режим";
        state.snapshot = alerts::demo_snapshot(directory.locations);
        state.snapshot.validated_ms = now_ms();
        store->publish(std::move(state));
        vTaskDelete(nullptr);
        return;
    }
    if (std::strlen(ALERTS_WIFI_SSID) == 0) {
        alerts::UiState state;
        state.connection = "Wi-Fi не задано у файлі збірки";
        store->publish(std::move(state));
        vTaskDelete(nullptr);
        return;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    network_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, nullptr));
    const wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    log_wifi_heap("before");
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init));
    log_wifi_heap("after");
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t wifi{};
    static_assert(sizeof(wifi.sta.ssid) == 32 && sizeof(wifi.sta.password) == 64);
    std::memcpy(wifi.sta.ssid, ALERTS_WIFI_SSID, std::strlen(ALERTS_WIFI_SSID));
    std::memcpy(wifi.sta.password, ALERTS_WIFI_PASSWORD, std::strlen(ALERTS_WIFI_PASSWORD));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp));
    alerts::Engine engine({ALERTS_PRIMARY_URL, ALERTS_RESERVE_URL, ALERTS_POLL_MS, ALERTS_STALE_MS},
                          directory.locations);
    alerts::KyivAlertSignal alert_signal(directory.locations);
    bool clock_ready = false;
    bool ota_checked = false;
    while (true) {
        if (!(xEventGroupGetBits(network_events) & connected_bit)) {
            auto state = engine.state(now_ms());
            state.connection = "Wi-Fi відключено · перепідключення";
            store->publish(std::move(state));
            esp_wifi_connect();
            xEventGroupWaitBits(network_events, connected_bit, pdFALSE, pdFALSE, pdMS_TO_TICKS(5000));
            continue;
        }
        if (!clock_ready) {
            auto state = engine.state(now_ms());
            state.connection = "Синхронізація часу";
            store->publish(std::move(state));
            clock_ready = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) == ESP_OK;
            if (!clock_ready) continue;
        }
        if (const auto request = engine.request(now_ms())) {
            const char* provider = request->provider == alerts::Provider::primary ? "Tryvoha.online" : "UA Siren";
            auto result = alerts_http_get(request->url);
            const bool http_ok = result.status == 200 && result.error.empty();
            if (!http_ok)
                ESP_LOGW("alerts", "request failed provider=%s status=%d error=%s",
                         provider, result.status, result.error.c_str());
            const auto completed_ms = now_ms();
            engine.complete(*request, std::move(result), completed_ms);
            auto state = engine.state(completed_ms);
            const auto& snapshot = state.snapshot;
            bool beep = false;
            if (snapshot.available && snapshot.validated_ms == completed_ms) {
                ESP_LOGI("alerts", "snapshot accepted provider=%s updated_at=%s records=%zu",
                         provider, snapshot.source_updated_at.empty() ? "-" : snapshot.source_updated_at.c_str(),
                         snapshot.events.size());
                beep = alert_signal.update(snapshot);
            } else if (http_ok) {
                ESP_LOGI("alerts", "snapshot not applied provider=%s detail=%s", provider, state.connection.c_str());
            }
            store->publish(std::move(state));
            if (beep) {
                ESP_LOGI("alerts", "New Kyiv or Kyiv oblast alert: buzzer pulse");
                ESP_ERROR_CHECK_WITHOUT_ABORT(alerts_board_beep());
            }
        }
        if (!ota_checked && engine.state(now_ms()).snapshot.available) {
            ota_checked = true;
            alerts_check_ota(store);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
}
void alerts_network_start(alerts::StateStore* store) {
    if (xTaskCreate(worker, "alerts_network", 12288, store, 4, nullptr) != pdPASS)
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
}
