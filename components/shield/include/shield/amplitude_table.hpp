// Stimulation current <-> AD5241 setting, from bench measurements (ADR-0010).
//
// Pure. The Howland source's transfer function depends on resistor ratios,
// the PA441 and the load, so it is measured, not computed: drive a known
// resistor at several pot settings, measure the current across R29, and list
// the pairs here. There is deliberately no built-in default table: a
// placeholder would look exactly like a measured one.

#ifndef NMT_SHIELD_AMPLITUDE_TABLE_HPP
#define NMT_SHIELD_AMPLITUDE_TABLE_HPP

#include <cstdint>
#include <vector>

namespace shield {

struct AmplitudePoint {
    float ma;
    std::uint8_t code;
};

class AmplitudeTable {
  public:
    AmplitudeTable() = default;

    // Points must be strictly increasing in both mA and code, with at least
    // two of them. If not, the table stays empty and valid() is false.
    explicit AmplitudeTable(std::vector<AmplitudePoint> points);

    bool valid() const { return !points_.empty(); }

    // Pot setting for a current, by linear interpolation, rounded to nearest.
    // False outside the measured range or if the table is invalid: the device
    // never extrapolates a current it has not measured.
    bool code_for_ma(float ma, std::uint8_t* code) const;

    // Inverse, for display. False outside the measured range.
    bool ma_for_code(std::uint8_t code, float* ma) const;

  private:
    std::vector<AmplitudePoint> points_;
};

}  // namespace shield

#endif  // NMT_SHIELD_AMPLITUDE_TABLE_HPP
