// Synthetic CMAP generator.
//
// Produces Train-of-Four twitch amplitudes for host development and for the
// desktop simulator, so the UI and state machine can be exercised with no
// board attached.

#ifndef NMT_HAL_MOCK_CMAP_SOURCE_HPP
#define NMT_HAL_MOCK_CMAP_SOURCE_HPP

#include "tof/tof_compute.hpp"

namespace hal_mock {

struct CmapParams {
    // T1 amplitude at full recruitment, in millivolts.
    float t1_amplitude_mv = 2.0f;

    // Fade per twitch: each twitch is this fraction of the one before, so the
    // resulting TOF Ratio is fade^3. 1.0 is no blockade.
    float fade = 1.0f;

    // Amplitude below which a twitch is suppressed entirely, simulating a
    // twitch dropping out under deepening blockade. Twitches vanish in reverse
    // order (T4 first) as fade falls, which is what real physiology does.
    float dropout_mv = 0.0f;

    // Stimulus amplitude at which recruitment plateaus, in mA. Below this the
    // response scales linearly; at or above it the response is full. Used to
    // exercise the calibration ramp.
    float supramaximal_ma = 15.0f;  // under the 30 mA shield limit with x1.3 margin (ADR-0010)
};

// Fills amps_mv[0..3] with a synthetic train at full stimulus.
void generate_train(const CmapParams& p, float amps_mv[tof::kTwitchCount]);

// Peak-to-peak T1 response for a single stimulus at current_ma, following a
// linear recruitment curve up to supramaximal_ma and flat above it.
float response_at(const CmapParams& p, float current_ma);

}  // namespace hal_mock

#endif  // NMT_HAL_MOCK_CMAP_SOURCE_HPP
