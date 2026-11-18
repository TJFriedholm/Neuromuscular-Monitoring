// hal::IStimulator on shield V1.0 (ESP-IDF only). ADR-0010, -0011, -0012.
//
// Output enable sequence:
//   enable_output(true):  OUTPUT_ENABLE high (releases the pot's SHDN).
//   enable_output(false): OUTPUT_ENABLE low first -- one GPIO write that forces
//                         STIM_LEVEL to 0 V and cannot fail on a stuck bus --
//                         then the pot set to 0 and read back.
// The PULSE line is only ever driven by the RMT one-shot, which ends on its own.

#ifndef NMT_SHIELD_SHIELD_STIMULATOR_HPP
#define NMT_SHIELD_SHIELD_STIMULATOR_HPP

#include <atomic>
#include <cstdint>

#include "hal/stimulator.hpp"
#include "shield/ad5241.hpp"
#include "shield/amplitude_table.hpp"
#include "shield/pulse_generator.hpp"

namespace shield {

class ShieldStimulator final : public hal::IStimulator {
  public:
    // `table` may be invalid (empty) until one has been measured; then
    // set_amplitude_ma() refuses everything and only set_amplitude_code() works.
    ShieldStimulator(Ad5241& pot, PulseGenerator& pulse, int output_enable_gpio,
                     AmplitudeTable table);

    // Drives OUTPUT_ENABLE low, then zeroes the pot. Call early in boot.
    esp_err_t init();

    // hal::IStimulator
    bool set_amplitude_ma(float ma) override;
    bool fire_pulse(std::uint32_t width_us) override;
    bool enable_output(bool on) override;
    bool output_enabled() const override { return output_on_; }
    bool faulted() const override { return faulted_; }

    // Raw pot setting, for measuring the amplitude table on the bench. Not part
    // of the HAL: the device sets current in mA.
    bool set_amplitude_code(std::uint8_t code);
    std::uint8_t amplitude_code() const { return code_; }

    // Latched like the device fault (ADR-0005); cleared only here.
    void clear_fault() { faulted_ = false; }

  private:
    void trip();

    Ad5241& pot_;
    PulseGenerator& pulse_;
    int enable_gpio_;
    AmplitudeTable table_;
    std::uint8_t code_ = 0;
    std::atomic<bool> output_on_{false};
    std::atomic<bool> faulted_{false};
};

}  // namespace shield

#endif  // NMT_SHIELD_SHIELD_STIMULATOR_HPP
