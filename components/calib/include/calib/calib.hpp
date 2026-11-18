// Automatic supramaximal calibration.
//
// Implements Design Specifications Rev 1 §5.7.11 Algorithm 2, with the
// corrections recorded in docs/adr/0003-calibration-gates-plateau-on-response.md.
//
// Pure: the ramp does not talk to hardware. The caller injects a measure
// function, so tests drive it with scripted recruitment curves.

#ifndef NMT_CALIB_HPP
#define NMT_CALIB_HPP

#include <cstddef>

namespace calib {

enum class Status {
    // A plateau was found. i_cal_ma is the operating amplitude.
    Ok,
    // The ramp reached i_max without ever evoking a detectable response.
    // Suggests a lead off, a misplaced electrode, or no nerve under the pad.
    NoResponse,
    // Responses were evoked but never stopped growing before i_max.
    // The supramaximal point is above the permitted range.
    NoPlateau,
    // Electrode impedance was outside the clinically acceptable window.
    BadImpedance,
};

struct Config {
    float i_start_ma = 10.0f;
    float i_step_ma = 5.0f;
    // Shield V1.0 limit, matching hal::kCurrentMaxMa (ADR-0010).
    float i_max_ma = 30.0f;

    // Plateau threshold: the response is considered to have stopped growing
    // when successive amplitudes differ by less than this, in millivolts.
    // No value is given anywhere in the spec; it needs bench data.
    float delta_mv = 0.0f;

    // Detection threshold in millivolts, shared with tof::compute. A response
    // below this is not on the recruitment curve, so a plateau in it is
    // meaningless -- this is the gate that stops a subthreshold reading being
    // mistaken for a plateau. See ADR-0003.
    float theta_mv = 0.0f;

    // Safety margin applied above the supramaximal threshold. §5.1.2 requires
    // 30%.
    float margin = 1.3f;

    float impedance_min_ohm = 500.0f;
    float impedance_max_ohm = 50000.0f;
};

struct Result {
    Status status;
    // Mean of the unstimulated EMG samples. A DC offset, subtracted before
    // amplitudes are measured. Valid whenever samples were supplied.
    float baseline_mv;
    // Operating amplitude. Meaningful only when status == Ok; 0 otherwise, so
    // a caller that ignores status cannot stimulate on a failed calibration.
    float i_cal_ma;
};

// Delivers one stimulus at current_ma and returns the peak-to-peak response in
// millivolts, baseline already removed. ctx is passed through untouched.
using MeasureFn = float (*)(float current_ma, void* ctx);

// baseline_samples/n may be null/0, in which case baseline_mv is 0.
Result run(const Config& cfg,
           float impedance_ohm,
           const float* baseline_samples,
           std::size_t n,
           MeasureFn measure,
           void* ctx);

}  // namespace calib

#endif  // NMT_CALIB_HPP
