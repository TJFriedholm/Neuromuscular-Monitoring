// Desktop simulator entry point.
//
// The same ui::App the device will run, in a 1280 x 720 SDL window.
// The mouse stands in for the touchscreen. Keys inject conditions, like the
// mockup's bench controls:
//
//   B  step the patient's blockade level (applies from the next Train-of-Four)
//   I  toggle electrode impedance in / out of range (checked at START)
//   F  raise a stimulator fault          A  acknowledge a latched fault
//
//   ./nmt_sim [--zoom <factor>] [--blockade <0-5>] [--page <name>]
//             [--fault] [--snapshot <file.bmp>]
//
// The window opens at the panel's physical size -- about 111 x 62 mm -- on
// the display it appears on, so text and touch targets can be judged as they
// will look on the device (ADR-0008). --zoom overrides that: --zoom 1 draws
// one panel pixel per screen point, a window about 2.5 times the real size.
//
// --snapshot starts monitoring, waits for the first Train-of-Four to be
// displayed, saves the screen as a BMP and exits. --blockade presses B that
// many times first; --page opens a page by name (menu, measurement, ...,
// entry for the exponent keypad); --fault raises a stimulator fault.

#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "lvgl.h"
#include "sdl_display.hpp"
#include "sim_device/sim_device.hpp"
#include "ui/app.hpp"

namespace {

constexpr int32_t kWidth = 1280;
constexpr int32_t kHeight = 720;

// The panel's physical width, from its nominal 5.0 in diagonal and 16:9
// pixel grid (ADR-0008: about 11.6 px/mm). Confirm against the datasheet's
// active area when the board arrives.
const float kPanelDiagonalMm = 5.0f * 25.4f;
const float kPanelWidthMm =
    kPanelDiagonalMm * kWidth / std::hypot(static_cast<float>(kWidth), static_cast<float>(kHeight));

sim::SimDevice g_device;

// Bench controls, like the mockup's.
void on_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_b:
            std::printf("blockade: %s\n", sim::name(g_device.next_blockade()));
            break;
        case SDLK_i:
            std::printf("impedance: %s\n", g_device.toggle_impedance() ? "in range" : "OUT OF RANGE");
            break;
        case SDLK_f:
            std::printf("fault raised: stimulator\n");
            g_device.raise_stimulator_fault();
            break;
        case SDLK_a:
            std::printf("fault acknowledged\n");
            g_device.acknowledge_fault();
            break;
        default:
            break;
    }
}

uint32_t sdl_ticks() { return SDL_GetTicks(); }

// Opens a page by name, for --page. Returns false for an unknown name.
bool open_page(ui::App& app, const char* name) {
    struct Named {
        const char* name;
        ui::Page page;
    };
    static const Named kPages[] = {
        {"monitor", ui::Page::Monitor},         {"signal", ui::Page::Signal},
        {"menu", ui::Page::Menu},               {"measurement", ui::Page::Measurement},
        {"stimulation", ui::Page::Stimulation}, {"electrodes", ui::Page::Electrodes},
        {"alarms", ui::Page::Alarms},           {"device", ui::Page::Device},
        {"about", ui::Page::About},             {"brightness", ui::Page::Brightness},
        {"choice", ui::Page::Choice},
    };
    if (std::strcmp(name, "entry") == 0) {
        app.go(ui::Page::Measurement);
        app.open_entry(ui::Field::Exponent);
        return true;
    }
    for (const Named& n : kPages) {
        if (std::strcmp(name, n.name) == 0) {
            app.go(n.page);
            return true;
        }
    }
    return false;
}

// Saves the active screen as a 24-bit BMP. LVGL's RGB888 is stored B, G, R,
// which is BMP's own byte order, so rows copy straight across. The image
// buffer comes from the system heap: at 2.7 MB it would not fit in LVGL's own
// 256 KB heap, which is sized for the device.
bool save_bmp(const char* path) {
    const uint32_t w = kWidth;
    const uint32_t h = kHeight;
    const uint32_t stride = w * 3;
    std::vector<uint8_t> pixels(stride * h);
    lv_draw_buf_t img;
    if (lv_draw_buf_init(&img, w, h, LV_COLOR_FORMAT_RGB888, stride, pixels.data(),
                         static_cast<uint32_t>(pixels.size())) != LV_RESULT_OK ||
        lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_RGB888, &img) !=
            LV_RESULT_OK) {
        return false;
    }
    const uint32_t row = (w * 3 + 3) & ~3u;
    const uint32_t size = 54 + row * h;
    uint8_t hdr[54] = {'B', 'M'};
    auto put32 = [&](int at, uint32_t v) {
        for (int i = 0; i < 4; ++i) hdr[at + i] = static_cast<uint8_t>(v >> (8 * i));
    };
    put32(2, size);
    put32(10, 54);
    put32(14, 40);
    put32(18, w);
    put32(22, h);
    hdr[26] = 1;
    hdr[28] = 24;
    put32(34, row * h);

    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) {
        return false;
    }
    std::fwrite(hdr, 1, sizeof hdr, f);
    static const uint8_t pad[3] = {};
    for (uint32_t y = h; y-- > 0;) {  // BMP rows run bottom-up
        std::fwrite(pixels.data() + y * stride, 1, w * 3, f);
        std::fwrite(pad, 1, row - w * 3, f);
    }
    std::fclose(f);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    // Print each line as it happens, even when output is not a terminal.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    float zoom = 0.0f;  // 0: actual size
    const char* snapshot = nullptr;
    const char* page = nullptr;
    bool fault = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--zoom") == 0 && i + 1 < argc) {
            zoom = std::strtof(argv[++i], nullptr);
        } else if (std::strcmp(argv[i], "--snapshot") == 0 && i + 1 < argc) {
            snapshot = argv[++i];
        } else if (std::strcmp(argv[i], "--page") == 0 && i + 1 < argc) {
            page = argv[++i];
        } else if (std::strcmp(argv[i], "--fault") == 0) {
            fault = true;
        } else if (std::strcmp(argv[i], "--blockade") == 0 && i + 1 < argc) {
            for (int n = std::atoi(argv[++i]); n > 0; --n) {
                g_device.next_blockade();
            }
        }
    }

    lv_init();
    lv_tick_set_cb(sdl_ticks);

    sim::SdlDisplay display;
    if (!display.open(kWidth, kHeight, kPanelWidthMm, zoom)) {
        return 1;
    }

    ui::App app;
    app.create(lv_screen_active(), g_device);
    if (page != nullptr && !open_page(app, page)) {
        std::fprintf(stderr, "unknown page: %s\n", page);
        return 2;
    }

    std::printf("NMT simulator. Click START. Keys: B blockade, I impedance, "
                "F fault, A acknowledge.\n");

    if (snapshot != nullptr) {
        g_device.start();
    }

    static int32_t trace[ui::kTracePoints];
    for (;;) {
        g_device.tick(SDL_GetTicks());
        app.show(g_device.snapshot());
        g_device.trace(trace, ui::kTracePoints);
        app.set_trace(trace, ui::kTracePoints);

        if (!display.poll(on_key)) {
            return 0;  // window closed
        }
        // Runs LVGL's timers, including redrawing what changed. Returns how
        // long until it next needs to run.
        const uint32_t idle_ms = lv_timer_handler();

        // Snapshot once the first result has been on screen for a moment,
        // so the EMG strip still shows the train that produced it.
        if (snapshot != nullptr && g_device.snapshot().monitor.has_measurement) {
            static uint32_t shown_at = SDL_GetTicks();
            if (fault) {
                g_device.raise_stimulator_fault();
                fault = false;
            }
            if (SDL_GetTicks() - shown_at > 300) {
                app.show(g_device.snapshot());
                lv_refr_now(nullptr);
                const bool ok = save_bmp(snapshot);
                std::printf("%s %s\n", ok ? "saved" : "FAILED to save", snapshot);
                return ok ? 0 : 1;
            }
        }
        SDL_Delay(idle_ms < 16 ? idle_ms : 16);
    }
}
