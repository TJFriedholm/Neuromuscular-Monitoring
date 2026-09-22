// Train-of-Four measurement from four twitch amplitudes.
//
// Implements Design Specifications Rev 1 §5.7.11 Algorithm 1, with the
// corrections recorded in docs/adr/0004-tof-ratio-validity-and-no-clamp.md.
//
// Pure: no global state, no hardware, no ESP-IDF. Builds and runs on the host.

#ifndef NMT_TOF_COMPUTE_HPP
#define NMT_TOF_COMPUTE_HPP

#include <cstdint>

namespace tof {

// Number of stimuli in a Train-of-Four.
inline constexpr int kTwitchCount = 4;

struct Result {
    // T4/T1. Meaningful only when ratio_valid is true; NaN otherwise, so that a
    // caller which ignores the flag produces something visibly wrong rather than
    // a plausible number. Deliberately NOT clamped to [0,1] -- see ADR-0004.
    float ratio;

    // Whether ratio could be computed at all. Separate from the value because
    // 0.0 is itself a legitimate ratio (profound fade), so it cannot double as
    // "unavailable".
    bool ratio_valid;

    // How many twitches exceeded the detection threshold, 0..4. Counts every
    // detectable twitch regardless of position, per Algorithm 1.
    std::uint8_t count;

    // Per-twitch detection, indexed T1..T4.
    bool detected[kTwitchCount];
};

// amps_mv: peak-to-peak CMAP amplitude of T1..T4, in millivolts, baseline
//          already removed.
// theta_mv: detection threshold in millivolts. A twitch is detectable when its
//          amplitude is strictly greater than theta.
//
// theta_mv is a response-side quantity. The §5.7.8 variable table gives it in
// mA, which is wrong -- milliamps describe the stimulus. See ADR-0004.
Result compute(const float amps_mv[kTwitchCount], float theta_mv);

}  // namespace tof

#endif  // NMT_TOF_COMPUTE_HPP
