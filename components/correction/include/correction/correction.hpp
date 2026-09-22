// Facial -> peripheral-equivalent TOF Ratio correction.
//
// Fills the gap left by Design Specifications Rev 1 §5.1.1 and §5.1.2, which
// require a "validated correction algorithm" and a displayed
// peripheral-equivalent ratio but define neither.
// See docs/adr/0006-correction-is-a-power-law.md.
//
// Pure. Isolated in its own component so that replacing the power law with a
// fitted model later touches one file.

#ifndef NMT_CORRECTION_HPP
#define NMT_CORRECTION_HPP

namespace correction {

// peripheral = facial ^ exponent.
//
// The exponent must be >= 1. Two properties follow, and both are load-bearing:
//
//   f(1) = 1  -- a fully recovered patient reads 1.00 at every site, because
//                fade has disappeared everywhere. A correction that moved this
//                endpoint would put the 0.90 extubation threshold out of reach
//                and the device could never indicate recovery at all.
//   f(x) <= x for x in [0,1] -- the correction only ever lowers the reported
//                value, which is the safe direction. The facial site recovers
//                sooner than the adductor pollicis, so it reads optimistically.
//
// A larger exponent is more conservative. The upper bound is a guard against
// a misconfiguration so pessimistic the device would be ignored.
inline constexpr float kExponentMin = 1.0f;
inline constexpr float kExponentMax = 4.0f;

// NMT_CORRECTION_DEFAULT_EXPONENT is deliberately not defined here. The value
// and its citation are outstanding from the team's research summary, and a
// placeholder would be indistinguishable in the code from a sourced value.
// The device build requires it (see CMakeLists.txt); host tests supply theirs.
#if defined(NMT_REQUIRE_CORRECTION_DEFAULT) && !defined(NMT_CORRECTION_DEFAULT_EXPONENT)
#error "NMT_CORRECTION_DEFAULT_EXPONENT is unset. Supply the facial-to-peripheral \
correction exponent and its citation before building for the device. See \
docs/adr/0006-correction-is-a-power-law.md."
#endif

enum class Status {
    Ok,
    // No exponent has been configured on this device yet.
    NotConfigured,
    // The supplied exponent is outside [kExponentMin, kExponentMax].
    InvalidExponent,
    // The Facial TOF Ratio was negative or not a number.
    InvalidInput,
    // The Facial TOF Ratio exceeded 1.0. T4 larger than T1 is not recovery --
    // it indicates post-tetanic facilitation or noise. No peripheral equivalent
    // is produced, because raising an already-anomalous reading toward or past
    // 1.00 would read as "fully recovered" on a measurement that is suspect.
    AnomalousInput,
};

struct Result {
    Status status;
    // Meaningful only when status == Ok; NaN otherwise.
    float peripheral_ratio;
};

// True when exponent lies within the permitted range. The settings UI uses this
// to bound its control, so an out-of-range value cannot be entered.
bool exponent_in_range(float exponent);

// The smallest Facial TOF Ratio that yields a peripheral ratio at or above
// `target` under this exponent -- i.e. what the facial site must read before
// the device will certify. Returns NaN if the exponent is out of range.
// The UI shows this so a clinician can see what the current setting demands.
float facial_needed_for(float target, float exponent);

// facial_ratio must be a valid TOF Ratio (tof::Result::ratio_valid). Callers
// pass the raw, unclamped value from tof::compute.
Result apply(float facial_ratio, float exponent);

}  // namespace correction

#endif  // NMT_CORRECTION_HPP
