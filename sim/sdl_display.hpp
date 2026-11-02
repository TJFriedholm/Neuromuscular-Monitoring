// A desktop window that stands in for the device's display and touchscreen.
//
// Replaces LVGL's own SDL driver, which creates its window without high-DPI
// support: on a Retina Mac that renders at half resolution and lets macOS
// stretch it, which is visibly blurry once the window is shrunk to the
// panel's physical size. Here the window is high-DPI, so the 1280 x 720 frame
// is scaled straight to the display's real pixels.

#ifndef NMT_SIM_SDL_DISPLAY_HPP
#define NMT_SIM_SDL_DISPLAY_HPP

#include <SDL.h>

#include <cstdint>
#include <vector>

#include "lvgl.h"

namespace sim {

class SdlDisplay {
  public:
    // Opens a window showing a width x height LVGL display, and registers the
    // mouse as its touchscreen. zoom is window points per panel pixel; 0 means
    // "the panel's physical size", as far as the host display reports it.
    bool open(int width, int height, float panel_width_mm, float zoom);

    // Handles pending window events. Key presses go to `on_key`. Returns false
    // once the window has been closed.
    bool poll(void (*on_key)(SDL_Keycode));

    float zoom() const { return zoom_; }

  private:
    static void flush(lv_display_t* disp, const lv_area_t* area, std::uint8_t* px);
    static void read_pointer(lv_indev_t* indev, lv_indev_data_t* data);

    int width_ = 0;
    int height_ = 0;
    float zoom_ = 1.0f;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    lv_display_t* display_ = nullptr;
    std::vector<std::uint8_t> frame_;  // RGB565, the whole screen
};

}  // namespace sim

#endif  // NMT_SIM_SDL_DISPLAY_HPP
