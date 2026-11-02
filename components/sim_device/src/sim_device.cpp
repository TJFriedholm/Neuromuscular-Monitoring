#include "sim_device/sim_device.hpp"

#include <cmath>
#include <cstdio>

#include "calib/calib.hpp"
#include "correction/correction.hpp"
#include "hal/impedance.hpp"
#include "tof/tof_compute.hpp"

namespace sim {
namespace {

using safety::Event;
using safety::State;

// ---- Simulator-only values. None of these is a clinical default. ----------

// Starting settings. The correction exponent is ILLUSTRATIVE ONLY: the device
// default and its citation are outstanding (ADR-0006); 2.0 matches the mockup.
// θ is provisional until bench values exist. All can be changed in the Menu.
ui::Settings initial_settings() {
    ui::Settings s;
    s.interval_s = 10;
    s.exponent = 2.0f;
    s.theta_mv = 0.10f;
    s.pulse_us = 250;
    s.alarm_limit = 0.90f;
    s.volume = ui::Volume::Medium;
    s.brightness_pct = 60;
    return s;
}

// Plateau threshold Δ for calibration. Provisional, like θ.
constexpr float kDeltaMv = 0.05f;

// Skin-electrode impedance the I key switches between.
constexpr float kImpedanceGoodOhm = 4200.0f;
constexpr float kImpedanceBadOhm = 62000.0f;

// 2 Hz, §5.7.11.
constexpr std::uint32_t kStimulusSpacingMs = 500;
// CMAP window after the last stimulus.
constexpr std::uint32_t kResponseWindowMs = 100;
// Alternating raises a fault on every this-many-th train since START.
constexpr int kAlternatingFaultEvery = 10;

// How long the electrode check and calibration appear to take. The work itself
// is instant here.
constexpr std::uint32_t kElectrodeCheckMs = 1000;
constexpr std::uint32_t kCalibrationMs = 2000;
// Width of the EMG strip, as in the mockup.
constexpr std::uint32_t kTraceWindowMs = 3000;

// T1 at full recruitment, from the mockup.
constexpr float kT1Mv = 2.04f;

// The ratio is fade^3, since T4 = T1 * fade^3.
float fade_for(Blockade b) {
    switch (b) {
        case Blockade::Alternating: return 0.955f;  // starts on Shallow; see begin_train
        case Blockade::Deep: return 0.20f;
        case Blockade::Moderate: return 0.85f;
        case Blockade::Shallow: return 0.955f;
        case Blockade::Recovered: return 0.99f;
        case Blockade::Anomalous: return 1.02f;
        case Blockade::None: return 1.0f;
    }
    return 1.0f;
}

float measure(float current_ma, void* ctx) {
    return hal_mock::response_at(*static_cast<const hal_mock::CmapParams*>(ctx), current_ma);
}

}  // namespace

const char* name(Blockade b) {
    switch (b) {
        case Blockade::Alternating: return "alternating (0.87 / 0.97)";
        case Blockade::Deep: return "deep";
        case Blockade::Moderate: return "moderate";
        case Blockade::Shallow: return "shallow";
        case Blockade::Recovered: return "recovered";
        case Blockade::Anomalous: return "anomalous (T4 > T1)";
        case Blockade::None: return "no response";
    }
    return "?";
}

SimDevice::SimDevice() : settings_(initial_settings()), impedance_ohm_(kImpedanceGoodOhm) {
    cmap_.t1_amplitude_mv = kT1Mv;
    cmap_.fade = fade_for(blockade_);
}

// ---- ui::Controller ---------------------------------------------------------

void SimDevice::reset_measurement() {
    has_measurement_ = false;
    state_since_ms_ = now_ms_;
}

void SimDevice::start() {
    if (sm_.state() != State::Idle) {
        return;
    }
    sm_.handle(Event::Start);
    operating_current_ma_ = 0.0f;
    trains_since_start_ = 0;
    alternate_recovered_ = false;
    reset_measurement();
}

void SimDevice::stop() {
    sm_.handle(Event::Stop);
    reset_measurement();
}

void SimDevice::recalibrate() {
    if (sm_.state() == State::Fault) {
        return;
    }
    stop();
    start();
}

void SimDevice::acknowledge_fault() {
    sm_.handle(Event::FaultAcknowledged);
    reset_measurement();
}

void SimDevice::set(ui::Field f, float value) {
    // The device checks bounds itself; the keypad is not the only guard.
    if (!ui::in_range(f, value)) {
        std::printf("rejected %s = %g (out of range)\n", ui::spec(f).title,
                    static_cast<double>(value));
        return;
    }
    switch (f) {
        case ui::Field::Interval: settings_.interval_s = static_cast<int>(std::lround(value)); break;
        case ui::Field::Exponent: settings_.exponent = value; break;
        case ui::Field::Theta: settings_.theta_mv = value; break;
        case ui::Field::PulseWidth: settings_.pulse_us = static_cast<int>(std::lround(value)); break;
        case ui::Field::AlarmLimit: settings_.alarm_limit = value; break;
        case ui::Field::Brightness:
            settings_.brightness_pct = static_cast<int>(std::lround(value));
            return;  // too chatty to log while dragging
    }
    std::printf("set %s = %g\n", ui::spec(f).title, static_cast<double>(value));
}

void SimDevice::set_volume(ui::Volume v) {
    settings_.volume = v;
    std::printf("set alarm volume = %s\n", ui::name(v));
}

// ---- Bench controls ---------------------------------------------------------

void SimDevice::raise_stimulator_fault() {
    sm_.handle(Event::FaultRaised, safety::FaultCause::Stimulator);
    state_since_ms_ = now_ms_;
}

Blockade SimDevice::next_blockade() {
    blockade_ = static_cast<Blockade>((static_cast<int>(blockade_) + 1) %
                                      (static_cast<int>(Blockade::None) + 1));
    cmap_.fade = fade_for(blockade_);
    cmap_.t1_amplitude_mv = blockade_ == Blockade::None ? 0.0f : kT1Mv;
    return blockade_;
}

bool SimDevice::toggle_impedance() {
    const bool ok = impedance_ohm_ != kImpedanceGoodOhm;
    impedance_ohm_ = ok ? kImpedanceGoodOhm : kImpedanceBadOhm;
    return ok;
}

// ---- Simulation -------------------------------------------------------------

void SimDevice::calibrate() {
    calib::Config cfg;
    cfg.theta_mv = settings_.theta_mv;
    cfg.delta_mv = kDeltaMv;
    const float baseline[8] = {};
    const calib::Result r =
        calib::run(cfg, impedance_ohm_, baseline, sizeof baseline / sizeof baseline[0], measure, &cmap_);
    switch (r.status) {
        case calib::Status::Ok:
            operating_current_ma_ = r.i_cal_ma;
            sm_.handle(Event::CalibrationOk);
            break;
        case calib::Status::NoResponse:
            sm_.handle(Event::CalibrationFailed, safety::FaultCause::CalibrationNoResponse);
            break;
        case calib::Status::NoPlateau:
            sm_.handle(Event::CalibrationFailed, safety::FaultCause::CalibrationNoPlateau);
            break;
        case calib::Status::BadImpedance:
            sm_.handle(Event::FaultRaised, safety::FaultCause::Impedance);
            break;
    }
}

void SimDevice::begin_train() {
    ++trains_since_start_;
    if (blockade_ == Blockade::Alternating) {
        if (trains_since_start_ % kAlternatingFaultEvery == 0) {
            std::printf("simulated electrode fault (train %d)\n", trains_since_start_);
            sm_.handle(Event::FaultRaised, safety::FaultCause::Impedance);
            state_since_ms_ = now_ms_;
            return;
        }
        cmap_.fade = fade_for(alternate_recovered_ ? Blockade::Recovered : Blockade::Shallow);
        alternate_recovered_ = !alternate_recovered_;
    }
    hal_mock::generate_train(cmap_, train_mv_);
    sm_.handle(Event::Start);  // Ready -> StimT1
    state_since_ms_ = now_ms_;
}

void SimDevice::tick(std::uint32_t now_ms) {
    now_ms_ = now_ms;
    const std::uint32_t in_state = now_ms_ - state_since_ms_;

    switch (sm_.state()) {
        case State::CheckElectrodes:
            if (in_state >= kElectrodeCheckMs) {
                sm_.handle(hal::impedance_in_range(impedance_ohm_) ? Event::ElectrodesOk
                                                                   : Event::ElectrodesBad);
                state_since_ms_ = now_ms_;
            }
            break;
        case State::Calibrate:
            if (in_state >= kCalibrationMs) {
                calibrate();
                state_since_ms_ = now_ms_;
                next_train_ms_ = now_ms_ + 1000;  // first measurement promptly
            }
            break;
        case State::Ready:
            if (now_ms_ >= next_train_ms_) {
                next_train_ms_ = now_ms_ + static_cast<std::uint32_t>(settings_.interval_s) * 1000;
                begin_train();
            }
            break;
        case State::StimT1:
        case State::StimT2:
        case State::StimT3:
        case State::StimT4: {
            // Stimulus k is delivered at k * 500 ms after the train starts.
            const int k = sm_.pulses_delivered();
            if (in_state >= static_cast<std::uint32_t>(k) * kStimulusSpacingMs) {
                pulses_[pulse_head_] = Pulse{now_ms_, train_mv_[k]};
                pulse_head_ = (pulse_head_ + 1) % kPulseHistory;
                sm_.handle(Event::PulseComplete);
            }
            break;
        }
        case State::WaitForResponses:
            if (in_state >= 3 * kStimulusSpacingMs + kResponseWindowMs) {
                sm_.handle(Event::ResponsesReady);
            }
            break;
        case State::Calculate:
            for (int i = 0; i < tof::kTwitchCount; ++i) {
                amps_mv_[i] = train_mv_[i];
            }
            tof_ = tof::compute(amps_mv_, settings_.theta_mv);
            correction_ = tof_.ratio_valid
                              ? correction::apply(tof_.ratio, settings_.exponent)
                              : correction::Result{correction::Status::InvalidInput, NAN};
            has_measurement_ = true;
            sm_.handle(Event::ComputeDone);
            break;
        case State::Display:
            sm_.handle(Event::DisplayDone);
            break;
        case State::Idle:
        case State::Fault:
            break;
    }
}

ui::Snapshot SimDevice::snapshot() const {
    ui::Snapshot s;
    ui::MonitorInput& in = s.monitor;
    in.state = sm_.state();
    in.impedance_ok = hal::impedance_in_range(impedance_ohm_);
    in.has_measurement = has_measurement_;
    for (int i = 0; i < tof::kTwitchCount; ++i) {
        in.amps_mv[i] = amps_mv_[i];
    }
    in.tof = tof_;
    in.correction = correction_;
    in.alarm_limit = settings_.alarm_limit;
    const std::int32_t wait = static_cast<std::int32_t>(next_train_ms_ - now_ms_);
    in.next_in_s = wait > 0 ? (wait + 999) / 1000 : 0;

    s.settings = settings_;
    s.impedance_ohm = impedance_ohm_;
    s.operating_current_ma = operating_current_ma_;
    s.fault_cause = sm_.fault().cause();
    s.serial = "SIM-0001";
    s.firmware = "simulator";
    return s;
}

void SimDevice::trace(std::int32_t* out_uv, std::size_t n) const {
    // Each evoked response is drawn as a short biphasic wave, widened far
    // beyond a real CMAP (a few ms) so it is visible across a 3 s strip.
    // Same shape as the UI mockup's trace.
    const bool live = sm_.state() != State::Idle && sm_.state() != State::Fault;
    for (std::size_t x = 0; x < n; ++x) {
        const float t_ms = static_cast<float>(now_ms_) -
                           static_cast<float>(kTraceWindowMs) *
                               (1.0f - static_cast<float>(x) / static_cast<float>(n - 1));
        float v_uv = 0.0f;
        for (const Pulse& p : pulses_) {
            if (p.at_ms == 0) {
                continue;
            }
            // Distance in mockup units (1000 across the strip).
            const float d = (t_ms - static_cast<float>(p.at_ms)) * 1000.0f / kTraceWindowMs;
            if (d > -70.0f && d < 70.0f) {
                v_uv += p.amp_mv * 1000.0f * std::exp(-(d * d) / 80.0f) * std::sin(d / 3.2f);
            }
        }
        if (live) {
            const float u = t_ms / 6.0f;
            v_uv += (std::sin(u * 0.9f) * 1.3f + std::sin(u * 2.1f) * 0.8f +
                     std::sin(u * 4.7f) * 0.45f) * 25.0f;
        }
        out_uv[x] = static_cast<std::int32_t>(v_uv);
    }
}

}  // namespace sim
