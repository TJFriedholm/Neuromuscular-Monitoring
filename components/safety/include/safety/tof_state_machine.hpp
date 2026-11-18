// Train-of-Four measurement state machine.
//
// Pure: a transition is a function of (state, event). No timers, no hardware,
// no ESP-IDF -- the caller drives it from timer callbacks and HAL results, so
// every transition is reachable in a host test.

#ifndef NMT_SAFETY_TOF_STATE_MACHINE_HPP
#define NMT_SAFETY_TOF_STATE_MACHINE_HPP

#include "safety/fault.hpp"

namespace safety {

enum class State {
    Idle,
    // Verifying electrode impedance is inside the acceptable window.
    CheckElectrodes,
    // Supramaximal calibration ramp. Stimulates.
    Calibrate,
    // Calibrated and idle between measurements.
    Ready,
    StimT1,
    StimT2,
    StimT3,
    StimT4,
    // All four stimuli delivered; waiting for the last CMAP window to close.
    WaitForResponses,
    Calculate,
    Display,
    // Latched. Stimulation disabled, stimulator output disabled (ADR-0011).
    Fault,
};

enum class Event {
    Start,
    Stop,
    ElectrodesOk,
    ElectrodesBad,
    CalibrationOk,
    CalibrationFailed,
    // One stimulus of the train has been delivered.
    PulseComplete,
    ResponsesReady,
    ComputeDone,
    DisplayDone,
    // Raised from anywhere. Always wins.
    FaultRaised,
    // The clinician acknowledged the latched fault.
    FaultAcknowledged,
};

// True in the states where the stimulator may deliver current. The single
// invariant the safety tests assert against every state.
bool stimulation_allowed(State s);

// True where the stimulator output may be enabled (IStimulator::enable_output).
// Exactly the stimulating states: output is enabled only for the duration of a
// sequence, so a stuck PULSE line between trains delivers nothing (ADR-0011,
// ADR-0012). The +80 V rail itself cannot be switched on shield V1.0.
bool output_allowed(State s);

class TofStateMachine {
  public:
    State state() const { return state_; }
    const FaultLatch& fault() const { return fault_; }

    // Index of the stimulus most recently delivered, 0..4. Reset on Start.
    int pulses_delivered() const { return pulses_delivered_; }

    // Applies the event and returns the resulting state. Unhandled events in a
    // given state are ignored, leaving the state unchanged -- a spurious event
    // can never move the machine somewhere unintended.
    State handle(Event e, FaultCause cause = FaultCause::None);

  private:
    State state_ = State::Idle;
    FaultLatch fault_;
    int pulses_delivered_ = 0;
};

}  // namespace safety

#endif  // NMT_SAFETY_TOF_STATE_MACHINE_HPP
