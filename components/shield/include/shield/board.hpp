// Shield V1.0 wiring, as read from the schematic (ADR-0010).
//
// Pure: plain integers, no ESP-IDF types, so host tests can include it.
// Everything marked "to verify" must be checked on the bench (continuity, I2C
// scan) before it is relied on. The bench firmware's `scan` and `adc_inputs`
// commands exist to do that.

#ifndef NMT_SHIELD_BOARD_HPP
#define NMT_SHIELD_BOARD_HPP

#include <cstdint>

namespace shield {

// ---- Host header (J1) -------------------------------------------------------

// I2C: J1 pins 4/6. Same bus, same pins and same port as the BSP's touch and
// audio bus (ADR-0013).
inline constexpr int kI2cPort = 1;  // CONFIG_BSP_I2C_NUM
inline constexpr int kI2cSdaGpio = 7;
inline constexpr int kI2cSclGpio = 8;

// To verify: J1 pin 35 -> HOST_STIM_PULSE, pin 37 -> HOST_STIM_OUTPUT_ENABLE.
// Neither is an ESP32-P4 strapping pin (those are GPIO34-38).
inline constexpr int kStimPulseGpio = 46;
inline constexpr int kStimOutputEnableGpio = 47;

// ---- I2C devices ------------------------------------------------------------

// ADS122C04: 0x40 when A0 = A1 = DGND (to verify).
inline constexpr std::uint8_t kAdcAddress = 0x40;
// AD5241: 0b0101100 when AD0 = AD1 = GND (to verify).
inline constexpr std::uint8_t kPotAddress = 0x2C;

// Both parts are fine at 400 kHz; the ADS122C04 and ISO1640 go faster, the
// AD5241 does not.
inline constexpr std::uint32_t kI2cSpeedHz = 400000;

// Per-transaction timeout. The longest legitimate transfer is a 4-byte ADC read
// (about 0.2 ms at 400 kHz); anything near this is a stuck bus (ADR-0013).
inline constexpr int kI2cTimeoutMs = 5;

// ---- Analog front end -------------------------------------------------------

// Which ADS122C04 input carries ADC_SIGNAL is not clear from the schematic
// (AIN0 or AIN1, to verify with `adc_inputs`: the right one sits near 2.5 V).
inline constexpr int kEmgAdcInput = 0;
// IMP_ADC_IN goes to AIN3.
inline constexpr int kImpedanceAdcInput = 3;

// AD8226 (1 + 49.4k/5.49k) x OPA2197 (1 + 40.2k/10k). Nominal; replace with a
// bench measurement.
inline constexpr float kFrontEndGain = 10.0f * 5.02f;

// STIM_OUT divider: 22.1k / (499k + 499k + 22.1k).
inline constexpr float kStimOutDivider = 22.1f / (499.0f + 499.0f + 22.1f);

}  // namespace shield

#endif  // NMT_SHIELD_BOARD_HPP
