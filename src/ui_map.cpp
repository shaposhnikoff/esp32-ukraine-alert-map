#include "ui_internal.hpp"
#include <cmath>

namespace alerts {
void Screen::select_tab(bool country) {
    country_ = country;
    const auto& mask = country ? geo::national_mask : geo::home_mask;
    size_t index = 0;
    for (size_t i = 0; i < mask.word_count; i += 2) {
        std::fill_n(mask_.begin() + static_cast<ptrdiff_t>(index), mask.runs[i], mask.runs[i + 1]);
        index += mask.runs[i];
    }
    lv_label_set_text(title_, country ? "Тривоги України" : "Київ та область");
    lv_obj_set_style_bg_color(home_tab_, lv_color_hex(country ? 0x23364B : 0x295849), 0);
    lv_obj_set_style_bg_color(country_tab_, lv_color_hex(country ? 0x295849 : 0x23364B), 0);
    palette_keys_.clear();
    set_zoom(256);
    draw_map();
    if (list_visible_) rebuild_list();
}
void Screen::select_area(size_t index) {
    selected_ = index;
    refresh_details();
    draw_map();
}
void Screen::draw_map() {
    const bool fresh = is_fresh(state_.snapshot, state_.now_ms, state_.stale_ms);
    std::vector<uint32_t> keys;
    std::vector<Status> statuses;
    keys.reserve(geo::area_count);
    statuses.reserve(geo::area_count);
    for (size_t i = 0; i < geo::area_count; ++i) {
        const auto status = status_for(state_.snapshot, ui::location(geo::areas[i]));
        statuses.push_back(status);
        keys.push_back(static_cast<uint32_t>(status.level) | (uint32_t(status.partial) << 8) |
                       (uint32_t(fresh) << 9) | (uint32_t(i == selected_) << 10));
    }
    if (keys == palette_keys_) return;
    palette_keys_ = std::move(keys);
    for (size_t p = 0; p < mask_.size(); ++p) {
        const auto id = mask_[p] & 255U;
        uint32_t color = ui::panel;
        if (id && id <= statuses.size()) {
            const auto& status = statuses[id - 1];
            color = ui::color(status.level);
            if (!fresh) color = ((color & 0xfefefeU) >> 1) + 0x18212BU;
            if (status.partial && ((p % 700 + p / 700) % 10 < 2)) color = 0x33475B;
            if (mask_[p] & 256U) color = id - 1 == selected_ ? 0xF3FAFF : 0x243C49;
        }
        colors_[p] = ui::rgb565(color);
    }
    lv_obj_invalidate(map_image_);
}
void Screen::set_zoom(int value) {
    const int next = std::clamp(value, 256, 768);
    if (next == 256) offset_x_ = offset_y_ = 0;
    else {
        offset_x_ = 350 - (350 - offset_x_) * next / zoom_;
        offset_y_ = 210 - (210 - offset_y_) * next / zoom_;
    }
    zoom_ = next;
    place_map();
}
void Screen::place_map() {
    offset_x_ = std::clamp(offset_x_, 700 - 700 * zoom_ / 256, 0);
    offset_y_ = std::clamp(offset_y_, 420 - 420 * zoom_ / 256, 0);
    lv_obj_set_pos(map_image_, offset_x_, offset_y_);
    lv_image_set_scale(map_image_, static_cast<uint32_t>(zoom_));
    size_t label_index = 0;
    for (size_t i = 0; i < geo::area_count; ++i) {
        const auto& area = geo::areas[i];
        if (!area.home) continue;
        auto* label = map_labels_[label_index];
        auto* line = map_lines_[label_index];
        if (country_) {
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
            ++label_index;
            continue;
        }
        lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(line, LV_OBJ_FLAG_HIDDEN);
        const auto name = std::string(area.slug) == "kyiv" ? "КИЇВ" : ui::short_name(area.name);
        lv_label_set_text(label, name.c_str());
        geo::Point anchor = area.label;
        const std::string slug = area.slug;
        if (slug == "visgorodskii-raion") anchor = {95, 80};
        else if (slug == "bucanskii-raion") anchor = {95, 174};
        else if (slug == "fastivskii-raion") anchor = {95, 260};
        else if (slug == "bilocerkivskii-raion") anchor = {100, 350};
        else if (slug == "brovarskii-raion") anchor = {570, 154};
        else if (slug == "borispilskii-raion") anchor = {570, 228};
        else if (slug == "obuxivskii-raion") anchor = {555, 326};
        const int width = slug == "kyiv" ? 100 : 170;
        const int x = offset_x_ + static_cast<int>(anchor.x * zoom_ / 256);
        const int y = offset_y_ + static_cast<int>(anchor.y * zoom_ / 256);
        lv_obj_set_width(label, width);
        lv_obj_set_pos(label, x - width / 2, y - 12);
        auto& points = label_lines_[label_index];
        points[0] = {static_cast<lv_value_precise_t>(offset_x_ + area.label.x * zoom_ / 256),
                     static_cast<lv_value_precise_t>(offset_y_ + area.label.y * zoom_ / 256)};
        points[1] = {static_cast<lv_value_precise_t>(x + (anchor.x < area.label.x ? width / 2 : -width / 2)),
                     static_cast<lv_value_precise_t>(y)};
        lv_line_set_points(line, points.data(), 2);
        if (slug == "kyiv") lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
        ++label_index;
    }
}
void Screen::dispatch_map(lv_event_t* event) {
    static_cast<Screen*>(lv_event_get_user_data(event))->map_event(event);
}
void Screen::map_event(lv_event_t* event) {
    const auto code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) drag_distance_ = 0;
    if (code == LV_EVENT_PRESSING && zoom_ > 256) {
        lv_point_t vector;
        lv_indev_get_vect(lv_indev_active(), &vector);
        offset_x_ += vector.x; offset_y_ += vector.y;
        drag_distance_ += std::abs(vector.x) + std::abs(vector.y);
        place_map();
    }
    if (code == LV_EVENT_CLICKED && drag_distance_ < 8) {
        lv_point_t point;
        lv_indev_get_point(lv_indev_active(), &point);
        lv_area_t bounds;
        lv_obj_get_coords(map_frame_, &bounds);
        const int x = (point.x - bounds.x1 - offset_x_) * 256 / zoom_;
        const int y = (point.y - bounds.y1 - offset_y_) * 256 / zoom_;
        if (x >= 0 && x < 700 && y >= 0 && y < 420) {
            const auto id = mask_[static_cast<size_t>(y * 700 + x)] & 255U;
            if (id && id <= geo::area_count) select_area(id - 1);
        }
    }
}
}  // namespace alerts
