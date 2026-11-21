#pragma once
#include "lvgl.h"
#include <SDL.h>
#include <cstdint>
#include <string>
#include <vector>

namespace alerts {
class Window {
public:
    Window();
    ~Window();
    bool events();
    void capture(const std::string& path);
private:
    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* texture_{};
    lv_display_t* display_{};
    lv_indev_t* pointer_{};
    int x_{};
    int y_{};
    bool pressed_{};
    std::vector<uint16_t> buffer_;
    static void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels);
    static void pointer(lv_indev_t* input, lv_indev_data_t* data);
};
}  // namespace alerts
