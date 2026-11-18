// AD5241 digital potentiometer encoding (ADI AD5241/AD5242 data sheet).
//
// Pure: no I/O.
//
// A write is three bytes: address, instruction, data. The instruction byte is
//   bit 7  A/B  RDAC select (AD5241 has one RDAC: always 0)
//   bit 6  RS   midscale reset
//   bit 5  SD   shutdown (same effect as the SHDN pin)
//   bit 4  O1   logic output 1 (unused on the shield)
//   bit 3  O2   logic output 2 (unused on the shield)
// A read returns one byte: the current wiper setting.
//
// On the shield, terminal A is ISO_3V3 and B is ISO_GND, so the wiper voltage
// (STIM_LEVEL) is code/256 of 3.3 V. SHDN is driven by STIM_OUTPUT_ENABLE:
// low opens A and shorts the wiper to B, forcing STIM_LEVEL to 0 V without
// losing the stored setting.

#ifndef NMT_SHIELD_AD5241_REGS_HPP
#define NMT_SHIELD_AD5241_REGS_HPP

#include <cstdint>

namespace shield::ad5241 {

struct Instruction {
    bool midscale_reset = false;
    bool shutdown = false;
};

constexpr std::uint8_t instruction(Instruction i = {}) {
    return static_cast<std::uint8_t>(((i.midscale_reset ? 1 : 0) << 6) |
                                     ((i.shutdown ? 1 : 0) << 5));
}

// Fraction of terminal A's voltage that appears on the wiper.
constexpr float wiper_fraction(std::uint8_t code) {
    return static_cast<float>(code) / 256.0f;
}

inline constexpr float kTerminalAVolts = 3.3f;  // ISO_3V3

constexpr float stim_level_volts(std::uint8_t code) {
    return wiper_fraction(code) * kTerminalAVolts;
}

}  // namespace shield::ad5241

#endif  // NMT_SHIELD_AD5241_REGS_HPP
