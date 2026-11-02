// Stimulator interface.
//
// Hardware sits behind this seam; nothing above it includes ESP-IDF headers.

#ifndef NMT_HAL_STIMULATOR_HPP
#define NMT_HAL_STIMULATOR_HPP

#include <cstdint>

namespace hal {

// Bounds from Design Specifications Rev 1 §5.1.2. Enforced here as well as in
// hardware, so a software defect cannot ask for something out of range.
inline constexpr float kCurrentMinMa = 0.0f;
inline constexpr float kCurrentMaxMa = 60.0f;
inline constexpr std::uint32_t kPulseWidthMinUs = 200;
inline constexpr std::uint32_t kPulseWidthMaxUs = 300;

class IStimulator {
  public:
    virtual ~IStimulator() = default;

    // Sets the constant-current amplitude. Rejects out-of-range values.
    virtual bool set_amplitude_ma(float ma) = 0;

    // Delivers exactly one pulse of the given width.
    //
    // Note the shape: a width, not a gate. There is deliberately no
    // set_gate(bool), because that would let a caller open the gate and time
    // the pulse in software -- and a CPU hang between opening and closing it
    // would drive sustained DC current into the patient's facial nerve. The
    // width is handed to a hardware one-shot and enforced in silicon, so the
    // pulse ends regardless of what the CPU is doing. Making the unsafe call
    // unexpressible is what keeps ADR-0001 true over time.
    virtual bool fire_pulse(std::uint32_t width_us) = 0;

    // Energises / de-energises the high-voltage stage.
    virtual bool enable_hv(bool on) = 0;
    virtual bool hv_enabled() const = 0;

    // True if the stimulator reports a hardware fault (compliance limit,
    // readback mismatch, fault line asserted).
    virtual bool faulted() const = 0;
};

}  // namespace hal

#endif  // NMT_HAL_STIMULATOR_HPP
