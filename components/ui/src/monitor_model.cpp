#include "ui/monitor_model.hpp"

#include <algorithm>
#include <cstdio>

namespace ui {
namespace {

// Placeholder for a value that cannot be shown. Two dashes keep the width of
// "0.00" roughly, so the layout does not jump when a value appears.
constexpr const char* kNoValue = "\xE2\x80\x94 \xE2\x80\x94";  // "— —"
constexpr const char* kDash = "\xE2\x80\x94";                  // "—"

// Below this a twitch amplitude is shown as a dash rather than "0.00".
constexpr float kShownMinMv = 0.0005f;

template <std::size_t N>
void put(char (&dst)[N], const char* src) {
    std::snprintf(dst, N, "%s", src);
}

bool calibrating(safety::State s) {
    return s == safety::State::CheckElectrodes || s == safety::State::Calibrate;
}

bool measuring(safety::State s) {
    switch (s) {
        case safety::State::StimT1:
        case safety::State::StimT2:
        case safety::State::StimT3:
        case safety::State::StimT4:
        case safety::State::WaitForResponses:
        case safety::State::Calculate:
            return true;
        default:
            return false;
    }
}

void present_state(const MonitorInput& in, MonitorView& v) {
    const bool fault = in.state == safety::State::Fault;
    const bool idle = in.state == safety::State::Idle;

    if (fault) {
        put(v.state, "Fault");
        v.state_tone = Tone::Alert;
    } else if (calibrating(in.state)) {
        put(v.state, "Calibrating");
        v.state_tone = Tone::Accent;
    } else if (idle) {
        put(v.state, "Idle");
        v.state_tone = Tone::Muted;
    } else {
        put(v.state, "Monitoring");
        v.state_tone = Tone::Good;
    }

    if (idle) {
        put(v.next, "Stopped");
    } else if (in.state == safety::State::Ready || in.state == safety::State::Display) {
        std::snprintf(v.next, sizeof v.next, "Next %d s", std::max(0, in.next_in_s));
    } else if (measuring(in.state)) {
        put(v.next, "Measuring");
    } else {
        put(v.next, "");
    }

    put(v.electrodes, in.impedance_ok ? "Electrodes OK" : "Check electrodes");
    v.electrodes_tone = in.impedance_ok ? Tone::Good : Tone::Alert;

    const bool running = !idle && !fault;
    v.start_enabled = !running;
    put(v.start_label, running ? "Running" : "START");
    v.stop_enabled = running;
}

void present_ratio(const MonitorInput& in, MonitorView& v) {
    put(v.peripheral, kNoValue);
    v.ratio_tone = Tone::Faint;
    v.show_chip = false;
    put(v.chip, "");
    v.show_facial = false;
    put(v.facial, "");
    put(v.ratio_note, "No measurement yet");

    if (!in.has_measurement) {
        return;
    }

    v.show_chip = true;
    if (!in.tof.ratio_valid) {
        put(v.chip, "No ratio");
        v.ratio_tone = Tone::Muted;
        put(v.ratio_note, in.tof.count == 0 ? "No response. Check placement."
                                            : "T4 absent. Follow the count.");
        return;
    }

    v.show_facial = true;
    std::snprintf(v.facial, sizeof v.facial, "%.2f", static_cast<double>(in.tof.ratio));

    switch (in.correction.status) {
        case correction::Status::Ok: {
            const float p = in.correction.peripheral_ratio;
            std::snprintf(v.peripheral, sizeof v.peripheral, "%.2f", static_cast<double>(p));
            if (p >= in.alarm_limit) {
                v.ratio_tone = Tone::Good;
                std::snprintf(v.chip, sizeof v.chip, "%.2f reached",
                              static_cast<double>(in.alarm_limit));
            } else {
                v.ratio_tone = Tone::Caution;
                std::snprintf(v.chip, sizeof v.chip, "Below %.2f",
                              static_cast<double>(in.alarm_limit));
            }
            break;
        }
        case correction::Status::AnomalousInput:
            // T4 > T1. No peripheral equivalent is produced (see correction.hpp).
            v.ratio_tone = Tone::Caution;
            put(v.chip, "Suspect");
            break;
        case correction::Status::NotConfigured:
        case correction::Status::InvalidExponent:
            v.ratio_tone = Tone::Caution;
            put(v.chip, "No exponent");
            break;
        case correction::Status::InvalidInput:
            v.ratio_tone = Tone::Caution;
            put(v.chip, "Suspect");
            break;
    }
}

void present_twitches(const MonitorInput& in, MonitorView& v) {
    if (in.has_measurement) {
        std::snprintf(v.count, sizeof v.count, "%u", static_cast<unsigned>(in.tof.count));
        v.count_tone = Tone::Normal;
    } else {
        put(v.count, kDash);
        v.count_tone = Tone::Faint;
    }

    for (int i = 0; i < tof::kTwitchCount; ++i) {
        auto& b = v.bars[i];
        std::snprintf(b.name, sizeof b.name, "T%d", i + 1);
        const float a = in.has_measurement ? in.amps_mv[i] : 0.0f;
        // A sliver stays visible at zero so an empty track still reads as a bar.
        b.fill = std::clamp(a / kBarFullScaleMv, 0.03f, 1.0f);
        b.detected = in.has_measurement && in.tof.detected[i];
        if (a > kShownMinMv) {
            std::snprintf(b.value, sizeof b.value, "%.2f", static_cast<double>(a));
        } else {
            put(b.value, kDash);
        }
    }
}

}  // namespace

MonitorView present(const MonitorInput& in) {
    MonitorView v{};
    present_state(in, v);
    present_ratio(in, v);
    present_twitches(in, v);
    return v;
}

}  // namespace ui
