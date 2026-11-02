// Firmware entry point: DEMO build.
//
// Runs the real UI (ui::App) and core logic on the board, driven by the same
// simulated patient as the desktop simulator (sim::SimDevice). No stimulator
// or EMG driver exists yet, so nothing here touches the patient side.
//
// Who runs what. ESP-IDF runs FreeRTOS, a small real-time OS: code runs in
// "tasks" (threads). esp_lvgl_adapter starts its own task that calls
// lv_timer_handler() to redraw the screen and read the touchscreen. LVGL is
// not thread-safe, so everything that touches LVGL or the device runs in
// that one task: app_main builds the UI under the adapter's lock, then an LVGL
// timer (on_frame) advances the device and redraws. The UI's button handlers
// run in the same task, so they and on_frame never race on the device.
//
// Bench controls, as in the simulator, typed into the serial monitor
// (idf.py monitor):  B blockade   I impedance   F fault   A acknowledge

#include <cstdint>

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "sim_device/sim_device.hpp"
#include "ui/app.hpp"

namespace {

const char* const kTag = "nmt";

// The panel refreshes at 30 Hz (ADR-0007); updating faster shows nothing new.
constexpr uint32_t kFramePeriodMs = 33;

// The serial console: the board's "UART" USB-C port.
constexpr uart_port_t kConsoleUart = static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM);

sim::SimDevice g_device;
ui::App g_app;
int g_brightness_pct = -1;

uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

// Bench controls, like the simulator's keys.
void on_key(char key) {
    switch (key) {
        case 'b':
        case 'B':
            std::printf("blockade: %s\n", sim::name(g_device.next_blockade()));
            break;
        case 'i':
        case 'I':
            std::printf("impedance: %s\n", g_device.toggle_impedance() ? "in range" : "OUT OF RANGE");
            break;
        case 'f':
        case 'F':
            std::printf("fault raised: stimulator\n");
            g_device.raise_stimulator_fault();
            break;
        case 'a':
        case 'A':
            std::printf("fault acknowledged\n");
            g_device.acknowledge_fault();
            break;
        default:
            break;
    }
}

// Runs in the LVGL task every kFramePeriodMs.
void on_frame(lv_timer_t*) {
    // Timeout 0: take whatever has arrived, never wait.
    uint8_t keys[16];
    const int n = uart_read_bytes(kConsoleUart, keys, sizeof keys, 0);
    for (int i = 0; i < n; ++i) {
        on_key(static_cast<char>(keys[i]));
    }

    g_device.tick(now_ms());
    const ui::Snapshot s = g_device.snapshot();
    g_app.show(s);

    static int32_t trace[ui::kTracePoints];
    g_device.trace(trace, ui::kTracePoints);
    g_app.set_trace(trace, ui::kTracePoints);

    // The Brightness page reaches the backlight.
    if (s.settings.brightness_pct != g_brightness_pct) {
        g_brightness_pct = s.settings.brightness_pct;
        bsp_display_brightness_set(g_brightness_pct);
    }
}

// Lets the serial monitor's keystrokes be read without blocking. ESP-IDF's
// console normally writes to the UART directly and cannot read; installing
// the UART driver gives it a receive buffer.
void start_console_input() {
    ESP_ERROR_CHECK(uart_driver_install(kConsoleUart, 256, 0, 0, nullptr, 0));
    uart_vfs_dev_use_driver(kConsoleUart);
}

}  // namespace

extern "C" void app_main(void) {
    start_console_input();

    // Landscape 1280 x 720, rotated 90° onto the portrait panel (ADR-0007).
    // The adapter rotates the image but not touch, so the GT911 driver maps
    // its portrait coordinates instead: (x, y) -> (y, 720 - x).
    bsp_display_cfg_t cfg = {};
    cfg.lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    cfg.rotation = ESP_LV_ADAPTER_ROTATE_90;
    cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL;
    cfg.touch_flags.swap_xy = 1;
    cfg.touch_flags.mirror_x = 1;
    cfg.touch_flags.mirror_y = 0;

    // Also probes the GT911 at 0x5D, then 0x14, and fails if neither answers.
    // Without touch there is no STOP, so the device must not run (ADR-0007).
    if (bsp_display_start_with_config(&cfg) == nullptr) {
        ESP_LOGE(kTag, "Display or touch controller did not start; not running. "
                       "See the BSP log above.");
        return;
    }

    // -1: wait as long as it takes. (Don't test the result: the BSP returns
    // the adapter's esp_err_t as a bool, so success reads as false.)
    bsp_display_lock(static_cast<uint32_t>(-1));
    g_app.create(lv_screen_active(), g_device);
    lv_timer_create(on_frame, kFramePeriodMs, nullptr);
    bsp_display_unlock();

    bsp_display_backlight_on();
    ESP_LOGI(kTag, "NMT demo running (simulated patient). Keys: B blockade, "
                   "I impedance, F fault, A acknowledge.");
}
