// What the Monitor screen shows, decided without drawing anything.
//
// present() turns a snapshot of device state into the text, tones and flags the
// Monitor screen displays. Every clinical display decision lives here -- which
// ratio is shown, when it is withheld, what colour it takes -- so it can be
// unit-tested on the host. monitor_screen.cpp only lays the result out.
//
// Pure (ADR-0002): no LVGL, no ESP-IDF, no globals.

#ifndef NMT_UI_MONITOR_MODEL_HPP
#define NMT_UI_MONITOR_MODEL_HPP

#include "correction/correction.hpp"
#include "safety/tof_state_machine.hpp"
#include "tof/tof_compute.hpp"

namespace ui {

// Twitch amplitude that fills a bar to the top. A display scale, taken from the
// UI mockup; it does not affect detection.
inline constexpr float kBarFullScaleMv = 2.2f;

// Snapshot of the device state the Monitor screen needs. Copied from the
// device state under a brief lock, never referenced (ADR-0002).
struct MonitorInput {
    safety::State state = safety::State::Idle;
    bool impedance_ok = true;

    // False until the first Train-of-Four since START has been computed.
    bool has_measurement = false;
    float amps_mv[tof::kTwitchCount] = {};
    tof::Result tof{};
    correction::Result correction{};

    float alarm_limit = 0.90f;
    // Seconds until the next Train-of-Four; shown in Ready and Display.
    int next_in_s = 0;
};

// Semantic colour of an element. The view maps these to the palette.
enum class Tone {
    Normal,
    Muted,    // secondary text
    Faint,    // placeholder or undetected
    Accent,   // in progress
    Good,     // at or above the alarm limit; electrodes OK
    Caution,  // below the alarm limit; suspect reading
    Alert,    // fault
};

struct MonitorView {
    char state[16];
    Tone state_tone;

    char electrodes[20];
    Tone electrodes_tone;

    // Empty while calibrating, when nothing useful can be said. Sized for any
    // int, so GCC can prove "Next %d s" never truncates.
    char next[20];

    // Peripheral-Equivalent TOF Ratio, the primary number on the screen.
    char peripheral[8];
    Tone ratio_tone;
    bool show_chip;
    char chip[20];

    // Facial TOF Ratio. The exponent it was raised to is shown in Settings,
    // not here (ADR-0009). When there is no facial ratio, ratio_note explains
    // why instead.
    bool show_facial;
    char facial[8];
    char ratio_note[40];

    char count[4];
    Tone count_tone;

    struct Bar {
        char name[4];
        char value[8];  // amplitude in mV, or a dash
        float fill;     // 0..1 of the bar height
        bool detected;
    } bars[tof::kTwitchCount];

    bool start_enabled;
    char start_label[8];
    bool stop_enabled;
};

MonitorView present(const MonitorInput& in);

}  // namespace ui

#endif  // NMT_UI_MONITOR_MODEL_HPP
