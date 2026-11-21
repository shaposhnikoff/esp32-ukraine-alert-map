#include "alerts/ui.hpp"
#include "alerts/geometry.hpp"
#include <iostream>
#include <stdexcept>
#include <SDL.h>
#include <cstdlib>
#include <cstring>

namespace {
lv_obj_t* find_text(lv_obj_t* root, const char* text) {
    if (lv_obj_check_type(root, &lv_label_class) &&
        std::string(lv_label_get_text(root)) == text) return root;
    const auto count = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < count; ++i) {
        if (auto* match = find_text(lv_obj_get_child(root, static_cast<int32_t>(i)), text)) return match;
    }
    return nullptr;
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}
int main() {
    lv_init();
    auto* display = lv_display_create(1024, 600);
    static uint16_t buffer[1024 * 40];
    static uint16_t frame[1024 * 600];
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t* area, uint8_t* pixels) {
        const int width = area->x2 - area->x1 + 1;
        for (int row = 0; row <= area->y2 - area->y1; ++row)
            std::memcpy(frame + (area->y1 + row) * 1024 + area->x1,
                        pixels + row * width * 2, static_cast<size_t>(width * 2));
        lv_display_flush_ready(d);
    });
    try {
        {
            alerts::Screen screen(lv_screen_active());
            require(find_text(lv_screen_active(), "Київ та область"), "startup is not Kyiv view");
            require(find_text(lv_screen_active(), "м. Київ"), "separate city card missing");
            auto* country = find_text(lv_screen_active(), "Вся Україна");
            require(country, "national tab missing");
            lv_obj_send_event(lv_obj_get_parent(country), LV_EVENT_CLICKED, nullptr);
            require(find_text(lv_screen_active(), "Тривоги України"), "national tab did not change screen");
            auto* home = find_text(lv_screen_active(), "Київ та область");
            require(home, "home tab missing");
            lv_obj_send_event(lv_obj_get_parent(home), LV_EVENT_CLICKED, nullptr);
            require(find_text(lv_screen_active(), "Київ та область"), "home navigation failed");
            alerts::UiState state;
            state.demo = true;
            state.snapshot.available = true;
            state.snapshot.validated_ms = 1;
            state.now_ms = 2;
            state.snapshot.supported = {"kyiv"};
            state.snapshot.events = {{"kyiv", "", alerts::Level::yellow, "2026-11-21T14:08:00Z"}};
            screen.update(state);
            require(find_text(lv_screen_active(), "ДЕМО · УМОВНІ ДАНІ"), "demo data not labeled");
            require(find_text(lv_screen_active(), "Жовта тривога"), "city yellow state not visible");
            for (const auto level : {alerts::Level::none, alerts::Level::yellow,
                                     alerts::Level::red, alerts::Level::unknown}) {
                state.snapshot.events[0].level = level;
                screen.update(state);
                lv_obj_update_layout(lv_screen_active());
                const char* text = level == alerts::Level::red ? "Червона тривога"
                    : level == alerts::Level::yellow ? "Жовта тривога"
                    : level == alerts::Level::none ? "Без тривоги" : "Немає даних";
                auto* label = find_text(lv_screen_active(), text);
                require(label && lv_obj_get_height(label) <= 48, "city status wraps outside its card");
                if (level == alerts::Level::red) {
                    if (const char* path = std::getenv("ALERTS_UI_CAPTURE")) {
                        lv_refr_now(display);
                        auto* surface = SDL_CreateRGBSurfaceWithFormatFrom(
                            frame, 1024, 600, 16, 2048, SDL_PIXELFORMAT_RGB565);
                        require(surface, "cannot create capture surface");
                        const int result = SDL_SaveBMP(surface, path);
                        SDL_FreeSurface(surface);
                        require(result == 0, "cannot write UI capture");
                    }
                }
            }
            state.now_ms = 180001;
            screen.update(state);
            require(find_text(lv_screen_active(), "ДАНІ ЗАСТАРІЛИ"), "stale state not visible");
        }
        lv_display_delete(display);
        lv_deinit();
        std::cout << "PASS actual LVGL startup, tabs, city state, demo and stale UI\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
