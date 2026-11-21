#include "platform.hpp"
#include "alerts_config.hpp"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

void alerts_check_ota(alerts::StateStore* store) {
    if (std::strlen(ALERTS_OTA_URL) == 0) return;
    auto state = store->read();
    state.connection = "Перевірка оновлення";
    store->publish(state);
    esp_http_client_config_t http{};
    http.url = ALERTS_OTA_URL;
    http.crt_bundle_attach = esp_crt_bundle_attach;
    http.timeout_ms = ALERTS_TIMEOUT_MS;
    esp_https_ota_config_t config{};
    config.http_config = &http;
    esp_https_ota_handle_t handle = nullptr;
    esp_err_t result = esp_https_ota_begin(&config, &handle);
    if (result == ESP_OK) {
        esp_app_desc_t proposed{};
        result = esp_https_ota_get_img_desc(handle, &proposed);
        if (result == ESP_OK &&
            std::strcmp(proposed.project_name, esp_app_get_description()->project_name) == 0 &&
            std::strcmp(proposed.version, esp_app_get_description()->version) != 0) {
            state.connection = "Завантаження оновлення";
            store->publish(state);
            do {
                result = esp_https_ota_perform(handle);
                vTaskDelay(pdMS_TO_TICKS(1));
            } while (result == ESP_ERR_HTTPS_OTA_IN_PROGRESS);
            if (result == ESP_OK && esp_https_ota_is_complete_data_received(handle)) {
                result = esp_https_ota_finish(handle);
                handle = nullptr;
                if (result == ESP_OK) esp_restart();
            }
        }
        if (handle) esp_https_ota_abort(handle);
    }
    if (result != ESP_OK) ESP_LOGW("alerts", "OTA check failed code=%s", esp_err_to_name(result));
    state.connection = result == ESP_OK ? "Оновлення не потрібне" : "Помилка оновлення";
    store->publish(std::move(state));
}
