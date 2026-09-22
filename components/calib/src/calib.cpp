#include "calib/calib.hpp"

#include <cmath>

namespace calib {

namespace {

float mean(const float* samples, std::size_t n) {
    if (samples == nullptr || n == 0) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        sum += samples[i];
    }
    return sum / static_cast<float>(n);
}

}  // namespace

Result run(const Config& cfg,
           float impedance_ohm,
           const float* baseline_samples,
           std::size_t n,
           MeasureFn measure,
           void* ctx) {
    Result out{};
    out.status = Status::NoResponse;
    out.baseline_mv = mean(baseline_samples, n);
    out.i_cal_ma = 0.0f;

    if (impedance_ohm < cfg.impedance_min_ohm || impedance_ohm > cfg.impedance_max_ohm) {
        out.status = Status::BadImpedance;
        return out;
    }
    if (measure == nullptr) {
        out.status = Status::NoResponse;
        return out;
    }

    // "The response stopped growing" only carries information once the response
    // has started. responded stays false until at least one amplitude clears
    // theta, and the plateau test below cannot fire before then -- which is
    // what stops a subthreshold reading at the start of the ramp being read as
    // a plateau. See ADR-0003.
    bool responded = false;
    float r_prev = 0.0f;

    for (float i_ma = cfg.i_start_ma; i_ma <= cfg.i_max_ma; i_ma += cfg.i_step_ma) {
        const float r = measure(i_ma, ctx);

        if (responded && r > cfg.theta_mv && std::fabs(r - r_prev) < cfg.delta_mv) {
            float i_cal = cfg.margin * i_ma;
            if (i_cal > cfg.i_max_ma) {
                i_cal = cfg.i_max_ma;
            }
            out.status = Status::Ok;
            out.i_cal_ma = i_cal;
            return out;
        }

        if (r > cfg.theta_mv) {
            responded = true;
        }
        r_prev = r;
    }

    // Exhausting the ramp is a distinct outcome from finding a plateau at the
    // top of it. Algorithm 2 conflated them and returned i_max either way,
    // leaving the device stimulating at maximum current on a calibration that
    // never succeeded. See ADR-0003.
    out.status = responded ? Status::NoPlateau : Status::NoResponse;
    out.i_cal_ma = 0.0f;
    return out;
}

}  // namespace calib
