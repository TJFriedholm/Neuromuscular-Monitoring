#include "hal_mock/cmap_source.hpp"

namespace hal_mock {

void generate_train(const CmapParams& p, float amps_mv[tof::kTwitchCount]) {
    float a = p.t1_amplitude_mv;
    for (int i = 0; i < tof::kTwitchCount; ++i) {
        amps_mv[i] = (a < p.dropout_mv) ? 0.0f : a;
        a *= p.fade;
    }
}

float response_at(const CmapParams& p, float current_ma) {
    if (current_ma <= 0.0f) {
        return 0.0f;
    }
    if (current_ma >= p.supramaximal_ma) {
        return p.t1_amplitude_mv;
    }
    return p.t1_amplitude_mv * (current_ma / p.supramaximal_ma);
}

}  // namespace hal_mock
