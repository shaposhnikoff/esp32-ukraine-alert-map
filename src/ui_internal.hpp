#pragma once
#include "alerts/ui.hpp"
#include "alerts/geometry.hpp"
#include <algorithm>
#include <ctime>

extern "C" {
LV_FONT_DECLARE(ui_font_18);
LV_FONT_DECLARE(ui_font_22);
LV_FONT_DECLARE(ui_font_30);
}
namespace alerts::ui {
constexpr uint32_t background = 0x101A27;
constexpr uint32_t panel = 0x182637;
constexpr uint32_t text = 0xE8F0F7;
constexpr uint32_t muted = 0x9BAEBE;
constexpr uint32_t accent = 0x61D1B6;
struct Rect { int x; int y; int w; int h; };
inline lv_obj_t* box(lv_obj_t* parent, Rect r) {
    auto* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, r.x, r.y);
    lv_obj_set_size(obj, r.w, r.h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(panel), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 12, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
inline lv_obj_t* label(lv_obj_t* parent, const char* value, Rect r) {
    auto* obj = lv_label_create(parent);
    lv_label_set_text(obj, value);
    lv_obj_set_pos(obj, r.x, r.y);
    lv_obj_set_width(obj, r.w);
    lv_obj_set_style_text_font(obj, &ui_font_18, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(text), 0);
    return obj;
}
inline lv_obj_t* button(lv_obj_t* parent, const char* value, Rect r) {
    auto* obj = lv_button_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, r.x, r.y);
    lv_obj_set_size(obj, r.w, r.h);
    lv_obj_set_style_radius(obj, 9, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x23364B), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x36536A), LV_STATE_PRESSED);
    auto* caption = label(obj, value, {0, 0, r.w, 24});
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(caption);
    return obj;
}
inline uint32_t color(Level level) {
    switch (level) {
        case Level::none: return 0x3B9878;
        case Level::yellow: return 0xD2AF4C;
        case Level::red: return 0xD36268;
        case Level::unknown: return 0x526174;
    }
    return 0x526174;
}
inline const char* status_text(Level level) {
    switch (level) {
        case Level::none: return "Без тривоги";
        case Level::yellow: return "Жовта тривога";
        case Level::red: return "Червона тривога";
        case Level::unknown: return "Немає даних";
    }
    return "Немає даних";
}
inline Location location(const geo::Area& area) { return {area.slug, area.parent, area.name}; }
inline std::string short_name(std::string name) {
    const auto suffix = name.find(" район");
    if (suffix != std::string::npos) name.erase(suffix);
    return name;
}
inline uint16_t rgb565(uint32_t color) {
    return static_cast<uint16_t>(((color >> 8) & 0xf800) | ((color >> 5) & 0x07e0) |
                                 ((color >> 3) & 0x001f));
}
}  // namespace alerts::ui
