#include "ui_internal.hpp"
#include "alerts/time.hpp"
#include <cstdio>

namespace alerts {
void Screen::refresh_details() {
    const bool fresh = is_fresh(state_.snapshot, state_.now_ms, state_.stale_ms);
    const char* banner = !state_.snapshot.available ? "ОЧІКУВАННЯ ДАНИХ"
        : !fresh ? "ДАНІ ЗАСТАРІЛИ" : state_.demo ? "ДЕМО · УМОВНІ ДАНІ" : "АКТУАЛЬНІ ДАНІ";
    lv_label_set_text(banner_, banner);
    lv_obj_set_style_text_color(banner_, lv_color_hex(fresh ? ui::accent : 0xD2AF4C), 0);
    lv_label_set_text(connection_, state_.connection.c_str());
    const auto city = status_for(state_.snapshot, {"kyiv", "", "м. Київ"});
    lv_label_set_text(city_status_, ui::status_text(city.level));
    lv_obj_set_style_text_color(city_status_, lv_color_hex(ui::color(city.level)), 0);
    const auto time = std::time(nullptr);
    if (time > 1600000000) {
        std::tm local{};
        localtime_r(&time, &local);
        char clock[16];
        std::strftime(clock, sizeof(clock), "%H:%M", &local);
        lv_label_set_text(clock_, clock);
    }
    unsigned red = 0, yellow = 0, unknown = 0;
    for (size_t i = 0; i < geo::area_count; ++i) {
        if (!geo::areas[i].home || std::string(geo::areas[i].slug) == "kyiv") continue;
        switch (status_for(state_.snapshot, ui::location(geo::areas[i])).level) {
            case Level::red: ++red; break;
            case Level::yellow: ++yellow; break;
            case Level::unknown: ++unknown; break;
            case Level::none: break;
        }
    }
    char summary[160];
    if (unknown == 7) std::snprintf(summary, sizeof(summary), "Немає актуальних даних");
    else if (!red && !yellow && !unknown) std::snprintf(summary, sizeof(summary), "Усі 7 районів без тривоги");
    else std::snprintf(summary, sizeof(summary), "%u червоних · %u жовтих\n%u районів без даних", red, yellow, unknown);
    lv_label_set_text(summary_, summary);
    const auto& area = geo::areas[selected_];
    const auto selected = status_for(state_.snapshot, ui::location(area));
    const auto name = ui::short_name(area.name);
    lv_label_set_text(selected_name_, name.c_str());
    std::string status = ui::status_text(selected.level);
    if (selected.partial) status += " · частково";
    lv_label_set_text(selected_status_, status.c_str());
    lv_obj_set_style_text_color(selected_status_, lv_color_hex(ui::color(selected.level)), 0);
    std::string started = "Початок: —";
    if (!selected.started_at.empty()) {
        if (const auto parsed = parse_utc(selected.started_at)) {
            const auto epoch = *parsed;
            std::tm local{};
            localtime_r(&epoch, &local);
            char value[32];
            std::strftime(value, sizeof(value), "Початок %H:%M", &local);
            started = value;
        }
    }
    lv_label_set_text(selected_time_, started.c_str());
    const char* provider = state_.snapshot.provider == Provider::reserve ? "UA Siren · резерв" : "Tryvoha.online";
    char source[256];
    const auto age = state_.snapshot.available && state_.now_ms >= state_.snapshot.validated_ms
        ? (state_.now_ms - state_.snapshot.validated_ms) / 1000 : 0;
    std::snprintf(source, sizeof(source), "%s · %s · %llu с",
                  provider, fresh ? "Оновлено" : "Дані недоступні / останні відомі", static_cast<unsigned long long>(age));
    lv_label_set_text(source_, source);
}
void Screen::rebuild_list() {
    const int32_t scroll = lv_obj_get_scroll_y(list_);
    lv_obj_clean(list_);
    int y = 8;
    for (size_t i = 0; i < geo::area_count; ++i) {
        const auto& area = geo::areas[i];
        if (!country_ && !area.home) continue;
        const auto status = status_for(state_.snapshot, ui::location(area));
        const std::string caption = std::string(area.name) + " · " + ui::status_text(status.level);
        auto* button = ui::button(list_, caption.c_str(), {10, y, 660, 48});
        lv_obj_set_user_data(button, const_cast<geo::Area*>(&area));
        lv_obj_add_event_cb(button, dispatch_action, LV_EVENT_CLICKED, this);
        y += 56;
    }
    lv_obj_update_layout(list_);
    lv_obj_scroll_to_y(list_, -scroll, LV_ANIM_OFF);
}
}  // namespace alerts
