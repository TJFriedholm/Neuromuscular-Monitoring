#include "safety/tof_state_machine.hpp"

namespace safety {

bool stimulation_allowed(State s) {
    switch (s) {
        case State::Calibrate:
        case State::StimT1:
        case State::StimT2:
        case State::StimT3:
        case State::StimT4:
            return true;
        default:
            return false;
    }
}

bool output_allowed(State s) {
    // Kept separate from stimulation_allowed so the two can diverge (e.g. a
    // settling lead-in) without every caller changing. Today they are equal.
    return stimulation_allowed(s);
}

State TofStateMachine::handle(Event e, FaultCause cause) {
    // A fault wins from every state, including Fault itself. Checked before the
    // per-state dispatch so that no state can accidentally shadow it.
    if (e == Event::FaultRaised) {
        fault_.raise(cause == FaultCause::None ? FaultCause::Internal : cause);
        state_ = State::Fault;
        return state_;
    }

    if (state_ == State::Fault) {
        // Latched: only an acknowledgement gets out. Stop is deliberately not
        // accepted -- it would clear the fault without anyone seeing it.
        if (e == Event::FaultAcknowledged) {
            fault_.acknowledge();
            pulses_delivered_ = 0;
            state_ = State::Idle;
        }
        return state_;
    }

    // Stop returns to Idle from any non-fault state.
    if (e == Event::Stop) {
        pulses_delivered_ = 0;
        state_ = State::Idle;
        return state_;
    }

    switch (state_) {
        case State::Idle:
            if (e == Event::Start) {
                pulses_delivered_ = 0;
                state_ = State::CheckElectrodes;
            }
            break;

        case State::CheckElectrodes:
            if (e == Event::ElectrodesOk) {
                state_ = State::Calibrate;
            } else if (e == Event::ElectrodesBad) {
                fault_.raise(FaultCause::Impedance);
                state_ = State::Fault;
            }
            break;

        case State::Calibrate:
            if (e == Event::CalibrationOk) {
                state_ = State::Ready;
            } else if (e == Event::CalibrationFailed) {
                fault_.raise(cause == FaultCause::None ? FaultCause::CalibrationNoResponse
                                                       : cause);
                state_ = State::Fault;
            }
            break;

        case State::Ready:
            if (e == Event::Start) {
                pulses_delivered_ = 0;
                state_ = State::StimT1;
            }
            break;

        case State::StimT1:
            if (e == Event::PulseComplete) {
                pulses_delivered_ = 1;
                state_ = State::StimT2;
            }
            break;

        case State::StimT2:
            if (e == Event::PulseComplete) {
                pulses_delivered_ = 2;
                state_ = State::StimT3;
            }
            break;

        case State::StimT3:
            if (e == Event::PulseComplete) {
                pulses_delivered_ = 3;
                state_ = State::StimT4;
            }
            break;

        case State::StimT4:
            if (e == Event::PulseComplete) {
                pulses_delivered_ = 4;
                state_ = State::WaitForResponses;
            }
            break;

        case State::WaitForResponses:
            if (e == Event::ResponsesReady) {
                state_ = State::Calculate;
            }
            break;

        case State::Calculate:
            if (e == Event::ComputeDone) {
                state_ = State::Display;
            }
            break;

        case State::Display:
            if (e == Event::DisplayDone) {
                state_ = State::Ready;
            }
            break;

        case State::Fault:
            // Handled above.
            break;
    }

    return state_;
}

}  // namespace safety
