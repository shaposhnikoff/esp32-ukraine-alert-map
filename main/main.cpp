#include "board.h"
#include "platform.hpp"
#include "alerts_config.hpp"
#include "esp_lv_adapter.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include <cstdlib>
#include <ctime>
#include <memory>

namespace {
alerts::StateStore store;
std::unique_ptr<alerts::Screen> screen;
void refresh(lv_timer_t*) {
    auto state = store.read();
    state.now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000);
    screen->update(std::move(state));
}
}
extern "C" void app_main(void) {
    setenv("TZ", ALERTS_TIMEZONE, 1);
    tzset();
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(alerts_board_start());
    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    screen = std::make_unique<alerts::Screen>(lv_screen_active());
    lv_timer_create(refresh, 1000, nullptr);
    esp_lv_adapter_unlock();
    ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    alerts_network_start(&store);
}
