#include "tof/tof_compute.hpp"

#include <limits>

namespace tof {

Result compute(const float amps_mv[kTwitchCount], float theta_mv) {
    Result r{};
    r.ratio = std::numeric_limits<float>::quiet_NaN();
    r.ratio_valid = false;
    r.count = 0;

    for (int i = 0; i < kTwitchCount; ++i) {
        r.detected[i] = amps_mv[i] > theta_mv;
        if (r.detected[i]) {
            ++r.count;
        }
    }

    // Algorithm 1 gates the ratio on T1 and T4 alone. T1 must also be non-zero
    // for the division to mean anything; theta above zero already implies this,
    // but the check is explicit so a zero or negative theta cannot divide by 0.
    const bool t1 = r.detected[0];
    const bool t4 = r.detected[kTwitchCount - 1];
    if (t1 && t4 && amps_mv[0] > 0.0f) {
        r.ratio = amps_mv[kTwitchCount - 1] / amps_mv[0];
        r.ratio_valid = true;
    }

    // No clamp to [0,1]. A ratio above unity is real (post-tetanic
    // facilitation, or noise near full recovery) and must reach the UI as an
    // anomaly rather than be flattened into a confident 1.00. See ADR-0004.
    return r;
}

}  // namespace tof
