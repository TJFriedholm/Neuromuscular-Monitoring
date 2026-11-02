#include "ui/menu_model.hpp"

#include <cstdio>

namespace ui {
namespace {

Tile page_tile(const char* name, Page p) {
    Tile t{};
    t.name = name;
    t.editable = true;
    t.value_tone = Tone::Normal;
    t.target.kind = TileTarget::Kind::OpenPage;
    t.target.page = p;
    return t;
}

// A tile that opens the keypad for `f`, showing its current value.
Tile field_tile(const char* name, Field f, const Snapshot& s) {
    Tile t{};
    t.name = name;
    t.editable = true;
    t.value_tone = Tone::Normal;
    t.target.kind = TileTarget::Kind::OpenEntry;
    t.target.field = f;
    const FieldSpec& fs = spec(f);
    const float v = value_of(s.settings, f);
    if (f == Field::Exponent) {
        std::snprintf(t.value, sizeof t.value, "n %.*f", fs.decimals, static_cast<double>(v));
    } else {
        std::snprintf(t.value, sizeof t.value, "%.*f%s%s", fs.decimals, static_cast<double>(v),
                      fs.unit[0] != '\0' ? " " : "", fs.unit);
    }
    return t;
}

}  // namespace

Page parent_of(Page p) {
    switch (p) {
        case Page::Measurement:
        case Page::Stimulation:
        case Page::Electrodes:
        case Page::Alarms:
        case Page::Device:
            return Page::Menu;
        case Page::About:
        case Page::Brightness:
            return Page::Device;
        case Page::Choice:
            return Page::Alarms;
        case Page::Monitor:
        case Page::Signal:
        case Page::Menu:
        case Page::Entry:
            return Page::Monitor;
    }
    return Page::Monitor;
}

const char* title_of(Page p) {
    switch (p) {
        case Page::Monitor: return "Monitor";
        case Page::Signal: return "Signal";
        case Page::Menu: return "Menu";
        case Page::Measurement: return "Measurement";
        case Page::Stimulation: return "Stimulation";
        case Page::Electrodes: return "Electrodes";
        case Page::Alarms: return "Alarms";
        case Page::Device: return "Device";
        case Page::About: return "About";
        case Page::Brightness: return "Brightness";
        case Page::Entry: return "Enter value";
        case Page::Choice: return "Alarm volume";
    }
    return "";
}

int tiles_for(Page p, const Snapshot& s, Tile out[kMaxTiles]) {
    int n = 0;
    switch (p) {
        case Page::Menu:
            out[n++] = page_tile("Measurement", Page::Measurement);
            out[n++] = page_tile("Stimulation", Page::Stimulation);
            out[n++] = page_tile("Electrodes", Page::Electrodes);
            out[n++] = page_tile("Alarms", Page::Alarms);
            out[n++] = page_tile("Device", Page::Device);
            out[n++] = page_tile("Signal", Page::Signal);
            break;
        case Page::Measurement:
            out[n++] = field_tile("Repeat interval", Field::Interval, s);
            out[n++] = field_tile("Correction exponent", Field::Exponent, s);
            out[n++] = field_tile("Detection threshold \xCE\xB8", Field::Theta, s);
            break;
        case Page::Stimulation: {
            out[n++] = field_tile("Pulse width", Field::PulseWidth, s);
            Tile cur{};
            cur.name = "Operating current";
            cur.editable = false;
            if (s.operating_current_ma > 0.0f) {
                std::snprintf(cur.value, sizeof cur.value, "%.1f mA",
                              static_cast<double>(s.operating_current_ma));
                cur.value_tone = Tone::Normal;
            } else {
                std::snprintf(cur.value, sizeof cur.value, "Not calibrated");
                cur.value_tone = Tone::Faint;
            }
            out[n++] = cur;
            Tile recal{};
            recal.name = "Recalibrate";
            std::snprintf(recal.value, sizeof recal.value, "Run now");
            recal.value_tone = Tone::Accent;
            recal.editable = true;
            recal.target.kind = TileTarget::Kind::Recalibrate;
            out[n++] = recal;
            break;
        }
        case Page::Alarms: {
            out[n++] = field_tile("Ratio alarm below", Field::AlarmLimit, s);
            Tile vol{};
            vol.name = "Alarm volume";
            std::snprintf(vol.value, sizeof vol.value, "%s", name(s.settings.volume));
            vol.value_tone = Tone::Normal;
            vol.editable = true;
            vol.target.kind = TileTarget::Kind::OpenChoice;
            out[n++] = vol;
            break;
        }
        case Page::Device: {
            Tile b = page_tile("Brightness", Page::Brightness);
            std::snprintf(b.value, sizeof b.value, "%d %%", s.settings.brightness_pct);
            out[n++] = b;
            Tile a = page_tile("About", Page::About);
            std::snprintf(a.value, sizeof a.value, "%s", s.serial);
            out[n++] = a;
            break;
        }
        default:
            break;
    }
    return n;
}

FaultText present_fault(safety::FaultCause cause, float impedance_ohm) {
    FaultText f{};
    switch (cause) {
        case safety::FaultCause::Impedance:
            std::snprintf(f.cause, sizeof f.cause, "Electrode impedance out of range");
            std::snprintf(f.detail, sizeof f.detail,
                          "Measured %.1f k\xCE\xA9. Allowed 0.5 \xE2\x80\x93 50 k\xCE\xA9.",
                          static_cast<double>(impedance_ohm / 1000.0f));
            break;
        case safety::FaultCause::CalibrationNoResponse:
            std::snprintf(f.cause, sizeof f.cause, "No response up to 60 mA");
            std::snprintf(f.detail, sizeof f.detail,
                          "Calibration evoked no response. Check electrode placement and lead "
                          "seating.");
            break;
        case safety::FaultCause::CalibrationNoPlateau:
            std::snprintf(f.cause, sizeof f.cause, "No plateau up to 60 mA");
            std::snprintf(f.detail, sizeof f.detail,
                          "The response kept growing to the limit, so no supramaximal current "
                          "could be set.");
            break;
        case safety::FaultCause::Stimulator:
            std::snprintf(f.cause, sizeof f.cause, "Stimulator hardware fault");
            std::snprintf(f.detail, sizeof f.detail,
                          "Output current did not match the commanded amplitude.");
            break;
        case safety::FaultCause::EmgHardware:
            std::snprintf(f.cause, sizeof f.cause, "EMG amplifier not responding");
            std::snprintf(f.detail, sizeof f.detail,
                          "The recording front end stopped returning data.");
            break;
        case safety::FaultCause::Internal:
        case safety::FaultCause::None:
            std::snprintf(f.cause, sizeof f.cause, "Internal error");
            std::snprintf(f.detail, sizeof f.detail, "A firmware self-check failed.");
            break;
    }
    return f;
}

}  // namespace ui
