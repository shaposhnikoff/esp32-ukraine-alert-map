#include "ui_internal.hpp"
#include <utility>

namespace alerts {
Screen::Screen(lv_obj_t* root) : root_(root), mask_(700 * 420), colors_(700 * 420) {
    lv_obj_remove_style_all(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(ui::background), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    title_ = ui::label(root_, "Київ та область", {20, 15, 460, 42});
    lv_obj_set_style_text_font(title_, &ui_font_30, 0);
    banner_ = ui::label(root_, "ОЧІКУВАННЯ ДАНИХ", {478, 26, 340, 26});
    lv_obj_set_style_text_color(banner_, lv_color_hex(ui::accent), 0);
    clock_ = ui::label(root_, "--:--", {914, 22, 90, 32});
    lv_obj_set_style_text_font(clock_, &ui_font_22, 0);
    home_tab_ = ui::button(root_, "Київ та область", {18, 68, 220, 44});
    country_tab_ = ui::button(root_, "Вся Україна", {248, 68, 192, 44});
    connection_ = ui::label(root_, "Підключення...", {462, 79, 276, 26});
    list_button_ = ui::button(root_, "Список районів", {780, 68, 226, 44});
    map_frame_ = ui::box(root_, {18, 128, 700, 420});
    lv_obj_set_style_radius(map_frame_, 12, 0);
    map_image_ = lv_canvas_create(map_frame_);
    lv_canvas_set_buffer(map_image_, colors_.data(), 700, 420, LV_COLOR_FORMAT_RGB565);
    lv_image_set_pivot(map_image_, 0, 0);
    lv_obj_add_flag(map_image_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(map_image_, dispatch_map, LV_EVENT_ALL, this);
    for (size_t i = 0; i < map_labels_.size(); ++i) {
        map_lines_[i] = lv_line_create(map_frame_);
        lv_obj_set_style_line_color(map_lines_[i], lv_color_hex(0x73958F), 0);
        lv_obj_set_style_line_width(map_lines_[i], 1, 0);
        map_labels_[i] = ui::label(map_frame_, "", {0, 0, 160, 26});
        lv_obj_set_style_text_align(map_labels_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(map_labels_[i], lv_color_hex(0xF5FAFD), 0);
        lv_obj_set_style_bg_color(map_labels_[i], lv_color_hex(ui::background), 0);
        lv_obj_set_style_bg_opa(map_labels_[i], LV_OPA_70, 0);
        lv_obj_set_style_radius(map_labels_[i], 5, 0);
        lv_obj_add_flag(map_labels_[i], LV_OBJ_FLAG_EVENT_BUBBLE);
    }
    zoom_in_ = ui::button(root_, "+", {658, 352, 44, 44});
    zoom_out_ = ui::button(root_, "-", {658, 404, 44, 44});
    reset_ = ui::button(root_, "1:1", {642, 486, 60, 44});
    ui::box(root_, {738, 128, 268, 118});
    city_badge_ = ui::label(root_, "м. Київ", {758, 144, 228, 30});
    lv_obj_set_style_text_font(city_badge_, &ui_font_22, 0);
    city_status_ = ui::label(root_, "Немає даних", {758, 184, 228, 42});
    lv_obj_set_style_text_font(city_status_, &ui_font_22, 0);
    ui::box(root_, {738, 260, 268, 112});
    auto* oblast = ui::label(root_, "Київська область", {758, 275, 232, 30});
    lv_obj_set_style_text_font(oblast, &ui_font_22, 0);
    summary_ = ui::label(root_, "Очікування даних", {758, 316, 228, 48});
    ui::box(root_, {738, 386, 268, 162});
    ui::label(root_, "Обраний район", {758, 398, 228, 24});
    selected_name_ = ui::label(root_, "", {758, 428, 228, 30});
    lv_obj_set_style_text_font(selected_name_, &ui_font_22, 0);
    selected_status_ = ui::label(root_, "", {758, 466, 228, 26});
    selected_time_ = ui::label(root_, "", {758, 505, 228, 34});
    source_ = ui::label(root_, "Tryvoha.online", {20, 555, 984, 22});
    lv_obj_set_style_text_color(source_, lv_color_hex(ui::muted), 0);
    const std::array<std::pair<const char*, Level>, 4> legend{{
        {"Немає тривоги", Level::none}, {"Жовта", Level::yellow},
        {"Червона", Level::red}, {"Немає даних", Level::unknown}}};
    int x = 20;
    for (const auto& [name, level] : legend) {
        auto* dot = ui::box(root_, {x, 583, 10, 10});
        lv_obj_set_style_bg_color(dot, lv_color_hex(ui::color(level)), 0);
        ui::label(root_, name, {x + 18, 577, 170, 24});
        x += level == Level::none ? 214 : 174;
    }
    list_ = ui::box(root_, {18, 128, 700, 420});
    lv_obj_add_flag(list_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
    for (auto* obj : {home_tab_, country_tab_, list_button_, zoom_in_, zoom_out_, reset_})
        lv_obj_add_event_cb(obj, dispatch_action, LV_EVENT_CLICKED, this);
    for (size_t i = 0; i < geo::area_count; ++i)
        if (std::string(geo::areas[i].slug) == "brovarskii-raion") selected_ = i;
    select_tab(false);
    update({});
}
Screen::~Screen() {
    lv_obj_clean(root_);
}
void Screen::update(UiState state) {
    state_ = std::move(state);
    refresh_details();
    draw_map();
}
void Screen::dispatch_action(lv_event_t* event) {
    auto* self = static_cast<Screen*>(lv_event_get_user_data(event));
    auto* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
    if (target == self->home_tab_) self->select_tab(false);
    else if (target == self->country_tab_) self->select_tab(true);
    else if (target == self->zoom_in_) self->set_zoom(self->zoom_ + 128);
    else if (target == self->zoom_out_) self->set_zoom(self->zoom_ - 128);
    else if (target == self->reset_) self->set_zoom(256);
    else if (target == self->list_button_) {
        self->list_visible_ = !self->list_visible_;
        if (self->list_visible_) { self->rebuild_list(); lv_obj_remove_flag(self->list_, LV_OBJ_FLAG_HIDDEN); }
        else lv_obj_add_flag(self->list_, LV_OBJ_FLAG_HIDDEN);
    } else {
        const auto* area = static_cast<const geo::Area*>(lv_obj_get_user_data(target));
        if (area) {
            self->select_area(static_cast<size_t>(area - geo::areas));
            self->list_visible_ = false;
            lv_obj_add_flag(self->list_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
}  // namespace alerts
