// Electrode impedance interface.
//
// On shield V1.0 there is no lead-off detection. Impedance is estimated from
// the voltage on STIM_OUT during a stimulus and the commanded current, so it
// covers the stimulating electrodes only, and is refreshed once per train, not
// continuously (ADR-0014, proposed). The recording electrodes have no
// impedance path.

#ifndef NMT_HAL_IMPEDANCE_HPP
#define NMT_HAL_IMPEDANCE_HPP

namespace hal {

// Clinically acceptable window, Design Specifications Rev 1 §5.1.2. At 30 mA
// the source saturates above about 2.6 kOhm, so readings near the top of this
// window mean "too high to stimulate" rather than a precise value.
inline constexpr float kImpedanceMinOhm = 500.0f;
inline constexpr float kImpedanceMaxOhm = 50000.0f;

class IImpedance {
  public:
    virtual ~IImpedance() = default;

    // Most recent stimulating-electrode impedance estimate in ohms, or a
    // negative value on error or before the first measurement.
    virtual float read_ohm() = 0;
};

inline bool impedance_in_range(float ohm) {
    return ohm >= kImpedanceMinOhm && ohm <= kImpedanceMaxOhm;
}

}  // namespace hal

#endif  // NMT_HAL_IMPEDANCE_HPP
