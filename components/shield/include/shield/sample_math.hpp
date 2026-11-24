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

// Millivolts at the electrodes for a differential EMG reading (AIN0 - AIN1).
//
// AIN1 is ISO_VCM, the midpoint the front end's output is centred on, so the
// differential result is already zero-centred: nothing to subtract. Measured
// against the internal 2.048 V reference, which does not move with ISO_5V.
constexpr float electrode_mv(std::int32_t code, float vref_v, float front_end_gain) {
    return ads122c04::code_to_volts(code, vref_v) / front_end_gain * 1000.0f;
}

// True at either end of the ADC's range: the input is beyond +/- Vref and the
// value is not a measurement. Expected during a stimulus artifact.
constexpr bool is_clipped(std::int32_t code) {
    return code >= 0x7FFFFF || code <= -0x800000;
}

// Time for one RDATA poll with the data counter: START, address, command,
// repeated START, address, 4 data bytes, STOP. About 66 SCL periods.
constexpr float rdata_transaction_us(std::uint32_t scl_hz) {
    return 66.0f * 1e6f / static_cast<float>(scl_hz);
}

// Whether polling every poll_period_us at scl_hz leaves at least half the
// period free for the task and the driver. 400 kHz fits a 400 us poll;
// 100 kHz (about 660 us per read) does not.
constexpr bool polling_fits(std::uint32_t scl_hz, std::uint32_t poll_period_us) {
    return scl_hz > 0 && 2.0f * rdata_transaction_us(scl_hz) <= static_cast<float>(poll_period_us);
}

}  // namespace shield

#endif  // NMT_SHIELD_SAMPLE_MATH_HPP
