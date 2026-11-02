// Clinician-adjustable settings, their bounds, and typed entry on the keypad.
//
// Every numeric setting is typed on the keypad, never dragged (UI mockup), and
// is accepted only inside its bounds. The device re-checks with in_range()
// before applying a value, so the keypad is not the only guard.
//
// Pure: no LVGL, no ESP-IDF.

#ifndef NMT_UI_SETTINGS_HPP
#define NMT_UI_SETTINGS_HPP

namespace ui {

enum class Field {
    Interval,    // time between Trains-of-Four, s
    Exponent,    // correction exponent (ADR-0006)
    Theta,       // detection threshold θ, mV
    PulseWidth,  // µs
    AlarmLimit,  // Peripheral-Equivalent TOF Ratio alarm limit
    Brightness,  // %
};

struct FieldSpec {
    const char* title;
    const char* unit;  // "" when unitless
    float min;
    float max;
    int decimals;
};

const FieldSpec& spec(Field f);

// True when `value` lies within the field's bounds (inclusive).
bool in_range(Field f, float value);

enum class Volume { Low, Medium, High };
inline constexpr int kVolumeCount = 3;
const char* name(Volume v);

// Current values. No clinical default lives here: the correction exponent and
// θ are outstanding (ADR-0006, bench data), so they start unset (0) and the
// device must supply them.
struct Settings {
    int interval_s = 0;
    float exponent = 0.0f;
    float theta_mv = 0.0f;
    int pulse_us = 0;
    float alarm_limit = 0.0f;
    Volume volume = Volume::Medium;
    int brightness_pct = 60;
};

float value_of(const Settings& s, Field f);

// ---- Keypad entry ----------------------------------------------------------

struct Entry {
    Field field = Field::Interval;
    // What has been typed. Empty means "nothing typed yet": the current value
    // is shown greyed and Enter is disabled.
    char buf[8] = {};
};

// Keypad presses. Each ignores a key that cannot apply: a seventh character, a
// second decimal point, or a decimal point on a whole-number field.
void press_digit(Entry& e, char digit);
void press_point(Entry& e);
void press_delete(Entry& e);

struct EntryCheck {
    bool valid;       // Enter may be pressed
    float value;      // meaningful when valid
    bool error;       // message is a complaint, not the allowed range
    char message[40]; // "Allowed 1.0 – 4.0", "Outside 1.0 – 4.0", ...
};

EntryCheck check(const Entry& e);

}  // namespace ui

#endif  // NMT_UI_SETTINGS_HPP
