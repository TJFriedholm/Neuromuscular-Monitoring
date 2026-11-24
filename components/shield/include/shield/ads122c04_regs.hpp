// ADS122C04 register and command encoding (TI SBAS751, sections 8.5-8.6).
//
// Pure: no I/O. The driver (ads122c04.hpp) sends what these functions build,
// and host tests check the bits.
//
// The chip has four 8-bit configuration registers:
//   0: MUX[7:4]  GAIN[3:1]  PGA_BYPASS[0]
//   1: DR[7:5]   MODE[4]    CM[3]  VREF[2:1]  TS[0]
//   2: DRDY[7]   DCNT[6]    CRC[5:4]  BCS[3]  IDAC[2:0]
//   3: I1MUX[7:5] I2MUX[4:2] (current sources; unused here, always 0)

#ifndef NMT_SHIELD_ADS122C04_REGS_HPP
#define NMT_SHIELD_ADS122C04_REGS_HPP

#include <cstdint>

namespace shield::ads122c04 {

// ---- Commands ---------------------------------------------------------------

inline constexpr std::uint8_t kCmdReset = 0x06;
inline constexpr std::uint8_t kCmdStartSync = 0x08;
inline constexpr std::uint8_t kCmdPowerdown = 0x02;
inline constexpr std::uint8_t kCmdRdata = 0x10;

constexpr std::uint8_t cmd_rreg(std::uint8_t reg) {
    return static_cast<std::uint8_t>(0x20 | ((reg & 0x3) << 2));
}
constexpr std::uint8_t cmd_wreg(std::uint8_t reg) {
    return static_cast<std::uint8_t>(0x40 | ((reg & 0x3) << 2));
}

// ---- Register fields --------------------------------------------------------

enum class Mux : std::uint8_t {
    Ain0Ain1 = 0x0,
    Ain0Ain2 = 0x1,
    Ain0Ain3 = 0x2,
    Ain1Ain0 = 0x3,
    Ain1Ain2 = 0x4,
    Ain1Ain3 = 0x5,
    Ain2Ain3 = 0x6,
    Ain3Ain2 = 0x7,
    Ain0Avss = 0x8,
    Ain1Avss = 0x9,
    Ain2Avss = 0xA,
    Ain3Avss = 0xB,
    RefMonitor = 0xC,   // (VREFP - VREFN) / 4
    AvddMonitor = 0xD,  // (AVDD - AVSS) / 4
    Shorted = 0xE,      // both inputs at (AVDD + AVSS) / 2, for offset
};

// Single-ended input n (0..3) measured against AVSS.
constexpr Mux single_ended(int ain) {
    return static_cast<Mux>(0x8 + (ain & 0x3));
}

enum class Vref : std::uint8_t {
    Internal2048 = 0x0,  // 2.048 V
    External = 0x1,      // REFP/REFN: not wired on shield V1.0
    AnalogSupply = 0x2,  // AVDD - AVSS (ISO_5V); for DC checks of 0-5 V inputs
};

// Data rate code DR[2:0]. Normal mode rates are 20, 45, 90, 175, 330, 600,
// 1000 SPS; turbo mode doubles each.
enum class DataRate : std::uint8_t {
    Dr20 = 0,
    Dr45 = 1,
    Dr90 = 2,
    Dr175 = 3,
    Dr330 = 4,
    Dr600 = 5,
    Dr1000 = 6,
};

// Defaults are the EMG sampling configuration (ADR-0014).
struct Config {
    Mux mux = Mux::Ain0Ain1;  // ADC_SIGNAL - ISO_VCM
    // Gain 1, 2, 4, ... 128 as GAIN[2:0] = log2(gain). Only 1, 2, 4 are valid
    // with the PGA bypassed.
    std::uint8_t gain_log2 = 0;
    // Bypass the PGA. Required for single-ended inputs referred to AVSS, which
    // would otherwise sit outside the PGA's common-mode range. Also used for
    // EMG, so a stimulus artifact near the rails cannot overdrive the PGA.
    bool pga_bypass = true;
    DataRate rate = DataRate::Dr1000;
    bool turbo = true;
    bool continuous = true;
    // +/- 2.048 V differential: about +/- 40 mV at the electrodes.
    Vref vref = Vref::Internal2048;
    // Prefix each result with an 8-bit conversion counter. This is how the
    // driver tells new data from data it has already read, and counts missed
    // conversions, with one transaction per poll (DRDY is not wired, ADR-0014).
    bool data_counter = true;
};

// False if the combination is one the datasheet forbids.
constexpr bool valid(const Config& c) {
    if (c.gain_log2 > 7) {
        return false;
    }
    if (c.pga_bypass && c.gain_log2 > 2) {
        return false;
    }
    if (c.vref == Vref::External) {
        return false;  // no reference wired to REFP/REFN on this board
    }
    return true;
}

constexpr std::uint8_t reg0(const Config& c) {
    return static_cast<std::uint8_t>((static_cast<std::uint8_t>(c.mux) << 4) |
                                     ((c.gain_log2 & 0x7) << 1) | (c.pga_bypass ? 1 : 0));
}

constexpr std::uint8_t reg1(const Config& c) {
    return static_cast<std::uint8_t>((static_cast<std::uint8_t>(c.rate) << 5) |
                                     ((c.turbo ? 1 : 0) << 4) | ((c.continuous ? 1 : 0) << 3) |
                                     (static_cast<std::uint8_t>(c.vref) << 1));
}

// CRC, burn-out current sources and IDACs off.
constexpr std::uint8_t reg2(const Config& c) {
    return static_cast<std::uint8_t>((c.data_counter ? 1 : 0) << 6);
}

constexpr std::uint8_t reg3(const Config&) {
    return 0;
}

// DRDY is reg2 bit 7, read-only, and ignored when comparing a readback.
inline constexpr std::uint8_t kReg2DrdyBit = 0x80;

constexpr float samples_per_second(DataRate r, bool turbo) {
    constexpr float kNormal[] = {20.0f, 45.0f, 90.0f, 175.0f, 330.0f, 600.0f, 1000.0f};
    return kNormal[static_cast<int>(r)] * (turbo ? 2.0f : 1.0f);
}

// ---- Data -------------------------------------------------------------------

inline constexpr float kFullScaleCounts = 8388608.0f;  // 2^23
inline constexpr float kInternalRefV = 2.048f;

// The ADC returns 24-bit two's complement, MSB first.
constexpr std::int32_t sign_extend_24(std::uint8_t b2, std::uint8_t b1, std::uint8_t b0) {
    const std::uint32_t u = (static_cast<std::uint32_t>(b2) << 16) |
                            (static_cast<std::uint32_t>(b1) << 8) | b0;
    return (u & 0x800000u) ? static_cast<std::int32_t>(u | 0xFF000000u)
                           : static_cast<std::int32_t>(u);
}

// Volts at the ADC input for a raw code.
constexpr float code_to_volts(std::int32_t code, float vref_v, std::uint8_t gain_log2 = 0) {
    return static_cast<float>(code) * vref_v /
           (kFullScaleCounts * static_cast<float>(1u << gain_log2));
}

// AVDD from a reading of the AVDD monitor taken with the internal reference.
constexpr float avdd_from_monitor(std::int32_t code) {
    return 4.0f * code_to_volts(code, kInternalRefV);
}

}  // namespace shield::ads122c04

#endif  // NMT_SHIELD_ADS122C04_REGS_HPP
