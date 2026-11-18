// EMG sample bookkeeping and scaling. Pure.

#ifndef NMT_SHIELD_SAMPLE_MATH_HPP
#define NMT_SHIELD_SAMPLE_MATH_HPP

#include <cstdint>

#include "shield/ads122c04_regs.hpp"

namespace shield {

// The ADS122C04's data counter is 8 bits and increments once per conversion.
// Comparing it with the previous read says what happened in between:
//   0      no new conversion (a duplicate read; discard)
//   1      exactly one new conversion (normal)
//   n > 1  n - 1 conversions were missed before this one
// The driver polls faster than the ADC converts, so 0 is routine and n > 1
// means the polling task was held up. A stall longer than 255 conversions
// (128 ms at 2000 SPS) aliases and cannot be detected this way.
constexpr int counter_advance(std::uint8_t previous, std::uint8_t current) {
    return static_cast<std::uint8_t>(current - previous);
}

// Millivolts at the electrodes for a single-ended EMG reading.
//
// The front end's output sits on ISO_VCM, which is a divider from ISO_5V. With
// the ADC referenced to that same supply, ISO_VCM is half of full scale
// whatever the supply's exact value, so subtracting vref/2 removes it.
// Resistor tolerance in R14/R16 leaves a small DC offset; the AC coupling and
// the core's baseline subtraction take care of it.
constexpr float electrode_mv(std::int32_t code, float vref_v, float front_end_gain) {
    const float v_adc = ads122c04::code_to_volts(code, vref_v);
    return (v_adc - vref_v * 0.5f) / front_end_gain * 1000.0f;
}

}  // namespace shield

#endif  // NMT_SHIELD_SAMPLE_MATH_HPP
