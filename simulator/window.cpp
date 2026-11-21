#include "window.hpp"
#include <stdexcept>

namespace alerts {
Window::Window() : buffer_(1024 * 40) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)) throw std::runtime_error(SDL_GetError());
    window_ = SDL_CreateWindow("Ukraine Alerts · ESP32-S3 7B · 1024x600",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 600, SDL_WINDOW_SHOWN);
    if (!window_) throw std::runtime_error(SDL_GetError());
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer_) throw std::runtime_error(SDL_GetError());
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB565,
                                 SDL_TEXTUREACCESS_STREAMING, 1024, 600);
    if (!texture_) throw std::runtime_error(SDL_GetError());
    lv_init();
    lv_tick_set_cb(SDL_GetTicks);
    display_ = lv_display_create(1024, 600);
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(display_, this);
    lv_display_set_buffers(display_, buffer_.data(), nullptr,
                          static_cast<uint32_t>(buffer_.size() * 2), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display_, flush);
    pointer_ = lv_indev_create();
    lv_indev_set_type(pointer_, LV_INDEV_TYPE_POINTER);
    lv_indev_set_user_data(pointer_, this);
    lv_indev_set_read_cb(pointer_, pointer);
    SDL_RaiseWindow(window_);
}
Window::~Window() {
    lv_indev_delete(pointer_);
    lv_display_delete(display_);
    lv_deinit();
    SDL_DestroyTexture(texture_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
}
void Window::flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    auto* self = static_cast<Window*>(lv_display_get_user_data(display));
    SDL_Rect rectangle{area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1};
    if (SDL_UpdateTexture(self->texture_, &rectangle, pixels, rectangle.w * 2))
        throw std::runtime_error(SDL_GetError());
    if (lv_display_flush_is_last(display)) {
        SDL_RenderClear(self->renderer_);
        SDL_RenderCopy(self->renderer_, self->texture_, nullptr, nullptr);
        SDL_RenderPresent(self->renderer_);
    }
    lv_display_flush_ready(display);
}
void Window::pointer(lv_indev_t* input, lv_indev_data_t* data) {
    auto* self = static_cast<Window*>(lv_indev_get_user_data(input));
    data->point = {self->x_, self->y_};
    data->state = self->pressed_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
bool Window::events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT: return false;
            case SDL_MOUSEMOTION: x_ = event.motion.x; y_ = event.motion.y; break;
            case SDL_MOUSEBUTTONDOWN: pressed_ = true; x_ = event.button.x; y_ = event.button.y; break;
            case SDL_MOUSEBUTTONUP: pressed_ = false; x_ = event.button.x; y_ = event.button.y; break;
            default: break;
        }
    }
    return true;
}
void Window::capture(const std::string& path) {
    auto* surface = SDL_CreateRGBSurfaceWithFormat(0, 1024, 600, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) throw std::runtime_error(SDL_GetError());
    const int read = SDL_RenderReadPixels(renderer_, nullptr, surface->format->format,
                                          surface->pixels, surface->pitch);
    const int saved = read ? -1 : SDL_SaveBMP(surface, path.c_str());
    SDL_FreeSurface(surface);
    if (read || saved) throw std::runtime_error(SDL_GetError());
}
}  // namespace alerts
