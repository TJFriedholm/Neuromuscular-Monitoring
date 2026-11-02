#include "ui/settings.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "correction/correction.hpp"

namespace ui {
namespace {

// Bounds from the UI mockup. The exponent's come from correction.hpp so the two
// cannot drift; pulse width is §5.1 (200-300 µs). θ's range is provisional
// until bench values exist.
constexpr FieldSpec kSpecs[] = {
    {"Repeat interval", "s", 10.0f, 60.0f, 0},
    {"Correction exponent", "", correction::kExponentMin, correction::kExponentMax, 1},
    {"Detection threshold \xCE\xB8", "mV", 0.02f, 0.60f, 2},  // θ
    {"Pulse width", "\xC2\xB5s", 200.0f, 300.0f, 0},          // µs
    {"Ratio alarm limit", "", 0.50f, 1.00f, 2},
    {"Brightness", "%", 0.0f, 100.0f, 0},
};

// Allows for binary floating point, so a typed "0.6" is not rejected against
// a bound of 0.6f.
constexpr float kTolerance = 1e-6f;

constexpr std::size_t kMaxChars = 6;

}  // namespace

const FieldSpec& spec(Field f) { return kSpecs[static_cast<int>(f)]; }

bool in_range(Field f, float value) {
    const FieldSpec& s = spec(f);
    return value >= s.min - kTolerance && value <= s.max + kTolerance;
}

const char* name(Volume v) {
    switch (v) {
        case Volume::Low: return "Low";
        case Volume::Medium: return "Medium";
        case Volume::High: return "High";
    }
    return "?";
}

float value_of(const Settings& s, Field f) {
    switch (f) {
        case Field::Interval: return static_cast<float>(s.interval_s);
        case Field::Exponent: return s.exponent;
        case Field::Theta: return s.theta_mv;
        case Field::PulseWidth: return static_cast<float>(s.pulse_us);
        case Field::AlarmLimit: return s.alarm_limit;
        case Field::Brightness: return static_cast<float>(s.brightness_pct);
    }
    return 0.0f;
}

void press_digit(Entry& e, char digit) {
    const std::size_t n = std::strlen(e.buf);
    if (digit < '0' || digit > '9' || n >= kMaxChars) {
        return;
    }
    if (n == 1 && e.buf[0] == '0') {
        e.buf[0] = digit;  // no leading zeros: "0" then "7" is "7"
        return;
    }
    e.buf[n] = digit;
    e.buf[n + 1] = '\0';
}

void press_point(Entry& e) {
    const std::size_t n = std::strlen(e.buf);
    if (spec(e.field).decimals == 0 || std::strchr(e.buf, '.') != nullptr) {
        return;
    }
    if (n == 0) {
        std::strcpy(e.buf, "0.");
    } else if (n < kMaxChars) {
        e.buf[n] = '.';
        e.buf[n + 1] = '\0';
    }
}

void press_delete(Entry& e) {
    const std::size_t n = std::strlen(e.buf);
    if (n > 0) {
        e.buf[n - 1] = '\0';
    }
}

EntryCheck check(const Entry& e) {
    const FieldSpec& s = spec(e.field);
    EntryCheck c{};
    std::snprintf(c.message, sizeof c.message, "Allowed %.*f \xE2\x80\x93 %.*f%s%s", s.decimals,
                  static_cast<double>(s.min), s.decimals, static_cast<double>(s.max),
                  s.unit[0] != '\0' ? " " : "", s.unit);

    const std::size_t n = std::strlen(e.buf);
    if (n == 0) {
        return c;
    }
    const char* point = std::strchr(e.buf, '.');
    const int decimals = point != nullptr ? static_cast<int>(n - (point - e.buf) - 1) : 0;
    c.value = std::strtof(e.buf, nullptr);
    c.valid = decimals <= s.decimals && in_range(e.field, c.value);

    // A trailing point is a number still being typed, not an error yet.
    if (!c.valid && e.buf[n - 1] != '.') {
        c.error = true;
        if (decimals > s.decimals) {
            std::snprintf(c.message, sizeof c.message, "At most %d decimal places", s.decimals);
        } else {
            std::snprintf(c.message, sizeof c.message, "Outside %.*f \xE2\x80\x93 %.*f",
                          s.decimals, static_cast<double>(s.min), s.decimals,
                          static_cast<double>(s.max));
        }
    }
    return c;
}

}  // namespace ui
