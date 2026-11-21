#include "window.hpp"
#include "alerts/ui.hpp"
#include "alerts/geometry.hpp"
#include "alerts/engine.hpp"
#include "alerts/store.hpp"
#include "alerts_config.hpp"
#include "connection.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
lv_obj_t* find_text(lv_obj_t* root, const char* text) {
    if (lv_obj_check_type(root, &lv_label_class) &&
        std::string(lv_label_get_text(root)) == text) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* found = find_text(lv_obj_get_child(root, static_cast<int32_t>(i)), text)) return found;
    return nullptr;
}
}
int main(int argc, char** argv) {
    try {
        std::string capture;
        bool country = false;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--capture" && i + 1 < argc) capture = argv[++i];
            else if (argument == "--country") country = true;
            else throw std::invalid_argument("Usage: alerts_simulator [--capture image.bmp] [--country]");
        }
        setenv("TZ", ALERTS_TIMEZONE, 1);
        tzset();
        alerts::Window window;
        alerts::Screen screen(lv_screen_active());
        alerts::StateStore store;
        alerts::Connection connection(store);
        if (country) {
            auto* tab = find_text(lv_screen_active(), "Вся Україна");
            if (!tab) throw std::runtime_error("National tab missing");
            lv_obj_send_event(lv_obj_get_parent(tab), LV_EVENT_CLICKED, nullptr);
        }
        const auto started = SDL_GetTicks64();
        uint64_t refreshed = 0;
        while (window.events()) {
            const auto now = SDL_GetTicks64();
            if (now >= refreshed) {
                auto state = store.read();
                state.now_ms = now;
                screen.update(std::move(state));
                refreshed = now + 1000;
            }
            const auto delay = lv_timer_handler();
            if (!capture.empty() && now - started > 1200 &&
                (store.read().snapshot.available || now - started >= 15000)) {
                auto state = store.read();
                state.now_ms = now;
                screen.update(std::move(state));
                lv_refr_now(nullptr);
                window.capture(capture);
                std::cout << "CAPTURED " << capture << '\n';
                return 0;
            }
            SDL_Delay(std::min<uint32_t>(delay, 10));
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Simulator failed: " << error.what() << '\n';
        return 1;
    }
}
