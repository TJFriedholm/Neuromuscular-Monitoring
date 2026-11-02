// The pages that fill the body of the screen. Internal to the ui component.
//
// Each page builds its widgets once in create() and updates them in show().
// `root` is the page's top object; App shows one page at a time.

#ifndef NMT_UI_PAGES_HPP
#define NMT_UI_PAGES_HPP

#include <cstddef>
#include <cstdint>

#include "lvgl.h"
#include "ui/app.hpp"
#include "ui/menu_model.hpp"
#include "ui/monitor_model.hpp"

namespace ui {

// Live EMG plot, shared by the Monitor strip and the Signal page.
lv_obj_t* make_trace(lv_obj_t* parent, lv_chart_series_t** series);

class MonitorPage {
  public:
    void create(lv_obj_t* body, App& app);
    void show(const MonitorView& v);
    void set_trace(const std::int32_t* uv, std::size_t n);
    lv_obj_t* root = nullptr;

  private:
    lv_obj_t* ratio_card_ = nullptr;
    lv_obj_t* peripheral_ = nullptr;
    lv_obj_t* chip_ = nullptr;
    lv_obj_t* facial_row_ = nullptr;
    lv_obj_t* facial_ = nullptr;
    lv_obj_t* ratio_note_ = nullptr;
    lv_obj_t* count_ = nullptr;
    lv_obj_t* fills_[tof::kTwitchCount] = {};
    lv_obj_t* names_[tof::kTwitchCount] = {};
    lv_obj_t* trace_ = nullptr;
    lv_chart_series_t* series_ = nullptr;
};

class SignalPage {
  public:
    void create(lv_obj_t* body);
    void show(const MonitorView& v);
    void set_trace(const std::int32_t* uv, std::size_t n);
    lv_obj_t* root = nullptr;

  private:
    lv_obj_t* trace_ = nullptr;
    lv_chart_series_t* series_ = nullptr;
    lv_obj_t* names_[tof::kTwitchCount] = {};
    lv_obj_t* values_[tof::kTwitchCount] = {};
};

// Menu and the settings pages made of tiles (Measurement, Stimulation,
// Alarms, Device).
class TilesPage {
  public:
    void create(lv_obj_t* body, App& app);
    void show(Page p, const Snapshot& s);
    lv_obj_t* root = nullptr;

  private:
    struct TileWidgets {
        lv_obj_t* button;
        lv_obj_t* name;
        lv_obj_t* value;
        lv_obj_t* chevron;
    };
    TileWidgets tiles_[kMaxTiles] = {};
    TileTarget targets_[kMaxTiles] = {};
    Page laid_out_for_ = Page::Monitor;
};

class ElectrodesPage {
  public:
    void create(lv_obj_t* body);
    void show(const Snapshot& s);
    lv_obj_t* root = nullptr;

  private:
    lv_obj_t* card_ = nullptr;
    lv_obj_t* value_ = nullptr;
    lv_obj_t* chip_ = nullptr;
};

class AboutPage {
  public:
    void create(lv_obj_t* body);
    void show(const Snapshot& s);
    lv_obj_t* root = nullptr;

  private:
    lv_obj_t* serial_ = nullptr;
    lv_obj_t* firmware_ = nullptr;
};

class BrightnessPage {
  public:
    void create(lv_obj_t* body, App& app);
    void show(const Snapshot& s);
    lv_obj_t* root = nullptr;

  private:
    lv_obj_t* value_ = nullptr;
    lv_obj_t* slider_ = nullptr;
    lv_obj_t* note_ = nullptr;
};

class ChoicePage {
  public:
    void create(lv_obj_t* body, App& app);
    void show(const Snapshot& s);
    lv_obj_t* root = nullptr;

  private:
    lv_obj_t* options_[kVolumeCount] = {};
    lv_obj_t* checks_[kVolumeCount] = {};
};

class EntryPage {
  public:
    void create(lv_obj_t* body, App& app);
    void show(const Snapshot& s, const Entry& e);
    lv_obj_t* root = nullptr;

  private:
    lv_obj_t* field_box_ = nullptr;
    lv_obj_t* typed_ = nullptr;
    lv_obj_t* unit_ = nullptr;
    lv_obj_t* message_ = nullptr;
    lv_obj_t* current_ = nullptr;
    lv_obj_t* help_ = nullptr;
    lv_obj_t* point_key_ = nullptr;
};

class FaultPage {
  public:
    void create(lv_obj_t* body);
    void show(const Snapshot& s);
    lv_obj_t* root = nullptr;

  private:
    lv_obj_t* cause_ = nullptr;
    lv_obj_t* detail_ = nullptr;
};

}  // namespace ui

#endif  // NMT_UI_PAGES_HPP
