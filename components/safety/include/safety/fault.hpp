// Latching fault state.
//
// Adds the fault path absent from Design Specifications Rev 1 §5.7.3.
// See docs/adr/0005-faults-latch-until-acknowledged.md.

#ifndef NMT_SAFETY_FAULT_HPP
#define NMT_SAFETY_FAULT_HPP

namespace safety {

enum class FaultCause {
    None,
    // Electrode impedance left the 500 ohm - 50 kohm window.
    Impedance,
    // The stimulator reported a hardware fault or failed a readback.
    Stimulator,
    // Calibration ramped without ever evoking a detectable response.
    CalibrationNoResponse,
    // Calibration evoked responses but found no plateau below i_max.
    CalibrationNoPlateau,
    // The EMG front end is unreachable or returned an error.
    EmgHardware,
    // A firmware invariant was violated.
    Internal,
};

// A fault is sticky. It survives the condition that caused it clearing, and is
// released only by an explicit clinician acknowledgement, so that current is
// never reapplied to a patient without a human deciding so. An electrode
// drifting across the impedance limit therefore cannot make the device chatter
// between stimulating and not.
class FaultLatch {
  public:
    // Latches the fault. The FIRST cause is retained, because it is the root
    // cause; later faults are usually consequences of it.
    void raise(FaultCause cause);

    // Clears the latch unconditionally. Only the UI's acknowledge control calls
    // this. It does not re-check the underlying condition -- if the condition
    // persists it will simply raise again on the next check, which is the
    // intended behaviour.
    void acknowledge();

    bool active() const { return active_; }
    FaultCause cause() const { return cause_; }

  private:
    bool active_ = false;
    FaultCause cause_ = FaultCause::None;
};

}  // namespace safety

#endif  // NMT_SAFETY_FAULT_HPP
