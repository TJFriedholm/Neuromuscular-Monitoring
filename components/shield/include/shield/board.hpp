// Shield V1.0 wiring, as read from the schematic (ADR-0010).
//
// Pure: plain integers, no ESP-IDF types, so host tests can include it.
// Everything marked "to verify" must be checked on the bench (continuity, I2C
// scan) before it is relied on. The bench firmware's `scan` and `adc_inputs`
// commands exist to do that.

#ifndef NMT_SHIELD_BOARD_HPP
#define NMT_SHIELD_BOARD_HPP

#include <cstdint>

#include "shield/ad5241_regs.hpp"
#include "shield/ads122c04_regs.hpp"

namespace shield {

// ---- I2C buses (ADR-0015) ---------------------------------------------------

// The Waveshare board's own bus: J1 pins 4/6 = GPIO7/8, owned by the BSP.
// Touch, both audio codecs and nothing of ours.
inline constexpr int kBoardI2cPort = 1;  // CONFIG_BSP_I2C_NUM
inline constexpr int kBoardI2cSdaGpio = 7;
inline constexpr int kBoardI2cSclGpio = 8;

// 7-bit addresses on the board bus (BSP 1.0.4 source; GT911 is one or the other).
inline constexpr std::uint8_t kGt911AddressA = 0x5D;
inline constexpr std::uint8_t kGt911AddressB = 0x14;
inline constexpr std::uint8_t kEs8311Address = 0x18;  // speaker codec
inline constexpr std::uint8_t kEs7210Address = 0x40;  // microphone ADC

constexpr bool on_board_bus(std::uint8_t addr) {
    return addr == kGt911AddressA || addr == kGt911AddressB || addr == kEs8311Address ||
           addr == kEs7210Address;
}

// The shield's own bus, on the P4's other I2C controller. The shield's
// HOST_I2C_SDA/SCL (J1 pins 4/6) are wired to Waveshare header pins 16/20
// (GPIO28/29) instead of header pins 4/6, which stay unconnected. Wired on
// the bench, 2026-11-24 (user-reported); to verify with `scan shield`.
// GPIO28/29: on the header, not strapping pins (34-38), not USB, not claimed
// by the BSP (I2C 7/8, I2S 9-13, LCD 26/27, SD 39-44, PA 53).
inline constexpr int kShieldI2cPort = 0;
inline constexpr int kShieldI2cSdaGpio = 28;  // header pin 16
inline constexpr int kShieldI2cSclGpio = 29;  // header pin 20

// Bring-up speed, and the speed EMG streaming needs (see polling_fits()).
// The AD5241 is rated to 400 kHz, so nothing on this bus goes faster.
inline constexpr std::uint32_t kI2cBringupHz = 100000;
inline constexpr std::uint32_t kI2cStreamHz = 400000;

// Per-transaction timeout. The longest legitimate transfer is a 4-byte ADC read
// (about 0.7 ms at 100 kHz); anything near this is a stuck bus.
inline constexpr int kI2cTimeoutMs = 5;

// ---- Stimulator lines (J1) --------------------------------------------------

// To verify: J1 pin 35 -> HOST_STIM_PULSE, pin 37 -> HOST_STIM_OUTPUT_ENABLE.
// Neither is an ESP32-P4 strapping pin (those are GPIO34-38).
inline constexpr int kStimPulseGpio = 46;
inline constexpr int kStimOutputEnableGpio = 47;

static_assert(kShieldI2cSdaGpio != kStimPulseGpio && kShieldI2cSdaGpio != kStimOutputEnableGpio &&
                  kShieldI2cSclGpio != kStimPulseGpio &&
                  kShieldI2cSclGpio != kStimOutputEnableGpio,
              "shield I2C pins must not be the stimulator lines");

// ---- I2C devices on the shield bus ------------------------------------------

// ADS122C04: 0x40 with A0 = A1 = ISO_GND. The same as the ES7210 on the board
// bus, which is why the shield has its own bus (ADR-0015).
inline constexpr std::uint8_t kAdcAddress = 0x40;
// AD5241: 0b0101100 with AD0 = AD1 = GND (to verify).
inline constexpr std::uint8_t kPotAddress = 0x2C;

static_assert(on_board_bus(kAdcAddress),
              "if this fails the ADC no longer collides; revisit ADR-0015");
static_assert(!on_board_bus(kPotAddress), "pot address collides with the board bus");

// ---- ADC inputs (schematic sheet 3; ADR-0014) -------------------------------

// AIN0 = ADC_SIGNAL (front-end output), AIN1 = ISO_VCM (its 2.5 V midpoint),
// AIN2 = IMP_ADC_IN (STIM_OUT divider), AIN3 unconnected. To verify with
// `adc_inputs`.
inline constexpr ads122c04::Mux kEmgMux = ads122c04::Mux::Ain0Ain1;
inline constexpr int kImpedanceAdcInput = 2;
inline constexpr int kUnconnectedAdcInput = 3;

// AD8226 (1 + 49.4k/5.49k) x OPA2197 (1 + 40.2k/10k). Nominal; replace with a
// bench measurement.
inline constexpr float kFrontEndGain = 10.0f * 5.02f;

// STIM_OUT divider: 22.1k / (499k + 499k + 22.1k).
inline constexpr float kStimOutDivider = 22.1f / (499.0f + 499.0f + 22.1f);

// ---- Stimulator amplitude, nominal ------------------------------------------

// Improved Howland source, ideal resistor ratios: mA through R29 per volt of
// STIM_CMD. CALCULATED, not measured: ignores wiper resistance and feedback
// loading. Use the bench table (AmplitudeTable) for anything real.
inline constexpr float kNominalMaPerVolt = 18.25f;

// Until a measured table exists, the bench refuses pot codes above this for
// pulses. Code 21 is about 4.9 mA nominal.
inline constexpr std::uint8_t kBenchMaxPulseCode = 21;

constexpr float nominal_ma_for_code(std::uint8_t code) {
    return ad5241::stim_level_volts(code) * kNominalMaPerVolt;
}

}  // namespace shield

#endif  // NMT_SHIELD_BOARD_HPP
