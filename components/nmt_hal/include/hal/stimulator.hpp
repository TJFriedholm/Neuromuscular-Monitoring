// Stimulator interface.
//
// Hardware sits behind this seam; nothing above it includes ESP-IDF headers.
//
// On shield V1.0 (ADR-0010) the stimulator is an AD5241 digital potentiometer
// setting the amplitude command, a TS5A3159 switch gating it (PULSE line), and
// a PA441 current source on an always-on +80 V rail. Two independent control
// lines cross the isolator: PULSE and OUTPUT_ENABLE.

#ifndef NMT_HAL_STIMULATOR_HPP
#define NMT_HAL_STIMULATOR_HPP

#include <cstdint>

namespace hal {

// Current bounds. The spec (§5.1.2) asks for 0-60 mA; shield V1.0 is designed
// for 30 mA, so the firmware limit follows the board (ADR-0010). Enforced here
// so a software defect cannot ask for something out of range.
inline constexpr float kCurrentMinMa = 0.0f;
inline constexpr float kCurrentMaxMa = 30.0f;

// Pulse width bounds, §5.1.2.
inline constexpr std::uint32_t kPulseWidthMinUs = 200;
inline constexpr std::uint32_t kPulseWidthMaxUs = 300;

class IStimulator {
  public:
    virtual ~IStimulator() = default;

    // Sets the constant-current amplitude through a bench-measured
    // mA -> potentiometer table. Rejects out-of-range values.
    virtual bool set_amplitude_ma(float ma) = 0;

    // Delivers exactly one pulse of the given width.
    //
    // Note the shape: a width, not a gate. There is deliberately no
    // set_gate(bool), because that would let a caller open the gate and time
    // the pulse in software -- and a CPU hang between opening and closing it
    // would drive sustained DC current into the patient's facial nerve. The
    // width is handed to the ESP32-P4's one-shot timer peripheral, so the
    // pulse ends regardless of what the CPU is doing. Making the unsafe call
    // unexpressible is what keeps ADR-0001 true over time. The shield has no
    // pulse-width limit of its own; ADR-0012 records the layers that back
    // this up.
    virtual bool fire_pulse(std::uint32_t width_us) = 0;

    // Enables / disables the stimulator output. Disabled means: PULSE low,
    // OUTPUT_ENABLE low (forces the amplitude command to 0 V), and the
    // potentiometer set to zero. It does NOT remove high voltage -- the +80 V
    // rail is always on and firmware cannot switch it (ADR-0011). Enable only
    // for the duration of a stimulation sequence.
    virtual bool enable_output(bool on) = 0;
    virtual bool output_enabled() const = 0;

    // True if the stimulator has detected a hardware fault: potentiometer
    // readback mismatch, no answer on the I2C bus, or the output at the
    // compliance limit. The shield has no fault line of its own.
    virtual bool faulted() const = 0;
};

}  // namespace hal

#endif  // NMT_HAL_STIMULATOR_HPP
