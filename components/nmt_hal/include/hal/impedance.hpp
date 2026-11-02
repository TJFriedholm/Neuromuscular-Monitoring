// Electrode impedance interface (ADS1299 lead-off detection, per §5.1.2).

#ifndef NMT_HAL_IMPEDANCE_HPP
#define NMT_HAL_IMPEDANCE_HPP

namespace hal {

// Clinically acceptable window, Design Specifications Rev 1 §5.1.2.
inline constexpr float kImpedanceMinOhm = 500.0f;
inline constexpr float kImpedanceMaxOhm = 50000.0f;

class IImpedance {
  public:
    virtual ~IImpedance() = default;

    // Skin-electrode impedance in ohms, or a negative value on error.
    virtual float read_ohm() = 0;
};

inline bool impedance_in_range(float ohm) {
    return ohm >= kImpedanceMinOhm && ohm <= kImpedanceMaxOhm;
}

}  // namespace hal

#endif  // NMT_HAL_IMPEDANCE_HPP
