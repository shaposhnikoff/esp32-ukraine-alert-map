#pragma once
#include "alerts/model.hpp"
#include "lvgl.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace alerts {
struct UiState {
    Snapshot snapshot;
    uint64_t now_ms{};
    uint64_t stale_ms{180000};
    std::string connection{"Підключення..."};
    bool demo{};
};
class Screen {
public:
    explicit Screen(lv_obj_t* root);
    ~Screen();
    void update(UiState state);
private:
    lv_obj_t* root_;
    lv_obj_t* title_{};
    lv_obj_t* banner_{};
    lv_obj_t* connection_{};
    lv_obj_t* clock_{};
    lv_obj_t* city_status_{};
    lv_obj_t* city_badge_{};
    lv_obj_t* summary_{};
    lv_obj_t* selected_name_{};
    lv_obj_t* selected_status_{};
    lv_obj_t* selected_time_{};
    lv_obj_t* source_{};
    lv_obj_t* map_frame_{};
    lv_obj_t* map_image_{};
    lv_obj_t* list_{};
    lv_obj_t* list_button_{};
    lv_obj_t* home_tab_{};
    lv_obj_t* country_tab_{};
    lv_obj_t* zoom_in_{};
    lv_obj_t* zoom_out_{};
    lv_obj_t* reset_{};
    std::array<lv_obj_t*, 8> map_labels_{};
    std::array<lv_obj_t*, 8> map_lines_{};
    std::array<std::array<lv_point_precise_t, 2>, 8> label_lines_{};
    std::vector<uint16_t> mask_;
    std::vector<uint16_t> colors_;
    std::vector<uint32_t> palette_keys_;
    UiState state_;
    size_t selected_{};
    bool country_{};
    bool list_visible_{};
    int zoom_{256};
    int offset_x_{};
    int offset_y_{};
    int drag_distance_{};
    void select_tab(bool country);
    void select_area(size_t index);
    void draw_map();
    void refresh_details();
    void rebuild_list();
    void set_zoom(int value);
    void place_map();
    void map_event(lv_event_t* event);
    static void dispatch_map(lv_event_t* event);
    static void dispatch_action(lv_event_t* event);
};
}  // namespace alerts
