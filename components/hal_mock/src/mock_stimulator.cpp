#include "hal_mock/mock_stimulator.hpp"

namespace hal_mock {

bool MockStimulator::set_amplitude_ma(float ma) {
    if (faulted_) {
        return false;
    }
    if (!(ma >= hal::kCurrentMinMa && ma <= hal::kCurrentMaxMa)) {
        return false;
    }
    amplitude_ma_ = ma;
    return true;
}

bool MockStimulator::fire_pulse(std::uint32_t width_us) {
    if (faulted_ || !hv_) {
        return false;
    }
    if (width_us < hal::kPulseWidthMinUs || width_us > hal::kPulseWidthMaxUs) {
        return false;
    }
    pulses_.push_back(FiredPulse{amplitude_ma_, width_us});
    return true;
}

bool MockStimulator::enable_hv(bool on) {
    if (faulted_ && on) {
        return false;
    }
    hv_ = on;
    return true;
}

void MockStimulator::reset() {
    amplitude_ma_ = 0.0f;
    hv_ = false;
    faulted_ = false;
    pulses_.clear();
}

}  // namespace hal_mock
