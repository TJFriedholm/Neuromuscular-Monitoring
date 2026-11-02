// The whole UI: header, the page in the middle, and the footer's controls.
//
// Layout follows the UI mockup at ADR-0008 sizes, for a 1280 x 720 landscape
// display. What each page shows is decided by the pure presenters
// (monitor_model, menu_model, settings); this class lays it out and tracks
// which page is open.
//
// LVGL is not thread-safe. Call every method from the thread that runs
// lv_timer_handler() (on the device, under the esp_lvgl_adapter lock).

#ifndef NMT_UI_APP_HPP
#define NMT_UI_APP_HPP

#include <cstddef>
#include <cstdint>
#include <memory>

#include "lvgl.h"
#include "ui/controller.hpp"
#include "ui/menu_model.hpp"
#include "ui/settings.hpp"

namespace ui {

// Points across the EMG strip.
inline constexpr std::size_t kTracePoints = 320;
// Vertical half-range of the EMG strip, in microvolts.
inline constexpr std::int32_t kTraceRangeUv = 2500;

class App {
  public:
    App();
    ~App();

    // Builds every page on `screen` (usually lv_screen_active()). `c` must
    // outlive the App.
    void create(lv_obj_t* screen, Controller& c);

    // Redraws from a snapshot. Cheap to call every frame: LVGL only repaints
    // what actually changed.
    void show(const Snapshot& s);

    // Replaces the EMG trace with `n` samples in microvolts, oldest first.
    void set_trace(const std::int32_t* samples_uv, std::size_t n);

    Page page() const { return page_; }

    // ---- Used by the pages --------------------------------------------------
    void go(Page p);
    void open_entry(Field f);
    void activate(const TileTarget& t);
    Entry& entry() { return entry_; }
    Controller& controller() { return *controller_; }

  private:
    struct Parts;
    std::unique_ptr<Parts> parts_;
    Controller* controller_ = nullptr;

    Page page_ = Page::Monitor;
    Entry entry_;
    // Where Back and Enter go from the keypad.
    Page entry_return_ = Page::Monitor;
};

}  // namespace ui

#endif  // NMT_UI_APP_HPP
