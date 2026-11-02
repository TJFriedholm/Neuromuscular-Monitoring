// Pages, navigation between them, and what the Menu and Fault pages show.
//
// Pure: no LVGL, no ESP-IDF. The page tree follows the UI mockup.

#ifndef NMT_UI_MENU_MODEL_HPP
#define NMT_UI_MENU_MODEL_HPP

#include "safety/fault.hpp"
#include "ui/monitor_model.hpp"
#include "ui/settings.hpp"

namespace ui {

enum class Page {
    Monitor,
    Signal,
    Menu,
    Measurement,
    Stimulation,
    Electrodes,
    Alarms,
    Device,
    About,
    Brightness,
    Entry,   // keypad for one Field; Back returns where it was opened from
    Choice,  // pick one option (alarm volume)
};

// Where Back goes. Entry has no fixed parent: the caller remembers it.
Page parent_of(Page p);
const char* title_of(Page p);

// Everything the UI shows, copied from device state under a brief lock
// (ADR-0002).
struct Snapshot {
    MonitorInput monitor;
    Settings settings;
    float impedance_ohm = 0.0f;
    // Operating current from Supramaximal Calibration; 0 until calibrated.
    float operating_current_ma = 0.0f;
    safety::FaultCause fault_cause = safety::FaultCause::None;
    const char* serial = "";
    const char* firmware = "";
};

// ---- Menu tiles --------------------------------------------------------------

// What pressing a tile does.
struct TileTarget {
    enum class Kind { None, OpenPage, OpenEntry, OpenChoice, Recalibrate };
    Kind kind = Kind::None;
    Page page = Page::Menu;
    Field field = Field::Interval;
};

struct Tile {
    const char* name;
    char value[24];
    Tone value_tone;
    bool editable;  // false: shows a value but cannot be pressed
    TileTarget target;
};

inline constexpr int kMaxTiles = 6;

// Fills `out` with the tiles of a Menu or settings page and returns how many.
// Returns 0 for pages that are not made of tiles.
int tiles_for(Page p, const Snapshot& s, Tile out[kMaxTiles]);

// ---- Latched fault -------------------------------------------------------------

struct FaultText {
    char cause[48];
    char detail[96];
};

FaultText present_fault(safety::FaultCause cause, float impedance_ohm);

}  // namespace ui

#endif  // NMT_UI_MENU_MODEL_HPP
