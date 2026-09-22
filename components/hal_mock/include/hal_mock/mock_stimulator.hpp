// Fake stimulator: records what was asked of it, enforces the same bounds as
// the real one, and can be told to fail.

#ifndef NMT_HAL_MOCK_STIMULATOR_HPP
#define NMT_HAL_MOCK_STIMULATOR_HPP

#include <cstdint>
#include <vector>

#include "hal/stimulator.hpp"

namespace hal_mock {

struct FiredPulse {
    float amplitude_ma;
    std::uint32_t width_us;
};

class MockStimulator final : public hal::IStimulator {
  public:
    bool set_amplitude_ma(float ma) override;
    bool fire_pulse(std::uint32_t width_us) override;
    bool enable_hv(bool on) override;
    bool hv_enabled() const override { return hv_; }
    bool faulted() const override { return faulted_; }

    // Test controls.
    void set_faulted(bool f) { faulted_ = f; }
    const std::vector<FiredPulse>& pulses() const { return pulses_; }
    float amplitude_ma() const { return amplitude_ma_; }
    void reset();

  private:
    float amplitude_ma_ = 0.0f;
    bool hv_ = false;
    bool faulted_ = false;
    std::vector<FiredPulse> pulses_;
};

}  // namespace hal_mock

#endif  // NMT_HAL_MOCK_STIMULATOR_HPP
