#include "sdl_display.hpp"

#include <cstdio>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace sim {
namespace {

// Screen points per millimetre on the display showing `win`, or 0 when the
// display does not report its physical size. SDL window sizes are in points,
// so this is what turns millimetres into a window size.
float points_per_mm(SDL_Window* win) {
#ifdef __APPLE__
    SDL_Rect r;
    if (SDL_GetDisplayBounds(SDL_GetWindowDisplayIndex(win), &r) != 0) {
        return 0.0f;
    }
    // SDL and CoreGraphics share the global point coordinate space, so the
    // display's centre identifies it to CoreGraphics.
    const CGPoint centre = CGPointMake(r.x + r.w / 2.0, r.y + r.h / 2.0);
    CGDirectDisplayID id;
    uint32_t found = 0;
    if (CGGetDisplaysWithPoint(centre, 1, &id, &found) != kCGErrorSuccess || found == 0) {
        return 0.0f;
    }
    const CGSize mm = CGDisplayScreenSize(id);  // from the display's EDID
    if (mm.width <= 0.0) {
        return 0.0f;
    }
    return static_cast<float>(CGDisplayBounds(id).size.width / mm.width);
#else
    (void)win;
    return 0.0f;
#endif
}

}  // namespace

bool SdlDisplay::open(int width, int height, float panel_width_mm, float zoom) {
    width_ = width;
    height_ = height;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    // Created hidden at full size, so the display it lands on can be measured
    // before it is shown at its final size.
    window_ = SDL_CreateWindow("NMT monitor simulator - 1280 x 720", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, width, height,
                               SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }

    if (zoom <= 0.0f) {
        const float ppmm = points_per_mm(window_);
        if (ppmm > 0.0f) {
            zoom = panel_width_mm * ppmm / static_cast<float>(width);
            std::printf("Actual size: %.1f x %.1f mm (display %.2f points/mm, zoom %.2f). "
                        "Use --zoom 1 for full pixels.\n",
                        static_cast<double>(panel_width_mm),
                        static_cast<double>(panel_width_mm * height / width),
                        static_cast<double>(ppmm), static_cast<double>(zoom));
        } else {
            zoom = 1.0f;
            std::printf("This display does not report its size; showing full pixels. "
                        "Use --zoom to scale.\n");
        }
    }
    zoom_ = zoom;
    SDL_SetWindowSize(window_, static_cast<int>(width * zoom + 0.5f),
                      static_cast<int>(height * zoom + 0.5f));
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return false;
    }
    int out_w = 0;
    int out_h = 0;
    SDL_GetRendererOutputSize(renderer_, &out_w, &out_h);
    std::printf("Window drawn at %d x %d screen pixels.\n", out_w, out_h);
    // The frame is scaled to the window's real pixels; smooth filtering keeps
    // 1 px lines such as card borders from disappearing when it shrinks.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                                 width, height);
    if (texture_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        return false;
    }

    // LVGL draws straight into a whole-screen buffer (direct mode); each flush
    // uploads it to the texture once the last changed area is drawn.
    display_ = lv_display_create(width, height);
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
    frame_.assign(static_cast<std::size_t>(width) * height * 2, 0);
    lv_display_set_buffers(display_, frame_.data(), nullptr, static_cast<uint32_t>(frame_.size()),
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_driver_data(display_, this);
    lv_display_set_flush_cb(display_, flush);

    lv_indev_t* pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, read_pointer);
    lv_indev_set_driver_data(pointer, this);
    lv_indev_set_display(pointer, display_);

    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
    return true;
}

void SdlDisplay::flush(lv_display_t* disp, const lv_area_t*, std::uint8_t*) {
    auto* self = static_cast<SdlDisplay*>(lv_display_get_driver_data(disp));
    if (lv_display_flush_is_last(disp)) {
        SDL_UpdateTexture(self->texture_, nullptr, self->frame_.data(), self->width_ * 2);
        SDL_RenderClear(self->renderer_);
        SDL_RenderCopy(self->renderer_, self->texture_, nullptr, nullptr);  // fills the window
        SDL_RenderPresent(self->renderer_);
    }
    lv_display_flush_ready(disp);
}

void SdlDisplay::read_pointer(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* self = static_cast<SdlDisplay*>(lv_indev_get_driver_data(indev));
    int x = 0;
    int y = 0;
    const std::uint32_t buttons = SDL_GetMouseState(&x, &y);  // window points
    data->point.x = static_cast<int32_t>(static_cast<float>(x) / self->zoom_);
    data->point.y = static_cast<int32_t>(static_cast<float>(y) / self->zoom_);
    const bool inside = SDL_GetMouseFocus() == self->window_;
    data->state = inside && (buttons & SDL_BUTTON_LMASK) != 0 ? LV_INDEV_STATE_PRESSED
                                                              : LV_INDEV_STATE_RELEASED;
}

bool SdlDisplay::poll(void (*on_key)(SDL_Keycode)) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            return false;
        }
        if (e.type == SDL_KEYDOWN && e.key.repeat == 0 && on_key != nullptr) {
            on_key(e.key.keysym.sym);
        }
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_EXPOSED) {
            lv_obj_invalidate(lv_screen_active());  // repaint after being uncovered
        }
    }
    return true;
}

}  // namespace sim
