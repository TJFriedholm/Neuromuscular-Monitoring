// Transition coverage for safety::TofStateMachine.

#include "safety/tof_state_machine.hpp"
#include "unity.h"

using safety::Event;
using safety::FaultCause;
using safety::State;
using safety::TofStateMachine;

void setUp() {}
void tearDown() {}

namespace {

// Drives the machine to Ready via the happy path.
TofStateMachine ready_machine() {
    TofStateMachine m;
    m.handle(Event::Start);
    m.handle(Event::ElectrodesOk);
    m.handle(Event::CalibrationOk);
    return m;
}

const State kAllStates[] = {
    State::Idle,    State::CheckElectrodes, State::Calibrate,        State::Ready,
    State::StimT1,  State::StimT2,          State::StimT3,           State::StimT4,
    State::WaitForResponses, State::Calculate, State::Display,       State::Fault,
};

// Replays the event sequence that lands the machine in the given state.
TofStateMachine machine_in(State target) {
    TofStateMachine m;
    if (target == State::Idle) return m;
    m.handle(Event::Start);
    if (target == State::CheckElectrodes) return m;
    if (target == State::Fault) {
        m.handle(Event::ElectrodesBad);
        return m;
    }
    m.handle(Event::ElectrodesOk);
    if (target == State::Calibrate) return m;
    m.handle(Event::CalibrationOk);
    if (target == State::Ready) return m;
    m.handle(Event::Start);
    if (target == State::StimT1) return m;
    m.handle(Event::PulseComplete);
    if (target == State::StimT2) return m;
    m.handle(Event::PulseComplete);
    if (target == State::StimT3) return m;
    m.handle(Event::PulseComplete);
    if (target == State::StimT4) return m;
    m.handle(Event::PulseComplete);
    if (target == State::WaitForResponses) return m;
    m.handle(Event::ResponsesReady);
    if (target == State::Calculate) return m;
    m.handle(Event::ComputeDone);
    return m;  // Display
}

}  // namespace

// --- startup path ---

static void test_starts_in_idle() {
    TofStateMachine m;
    TEST_ASSERT_EQUAL(State::Idle, m.state());
}

static void test_idle_start_goes_to_check_electrodes() {
    TofStateMachine m;
    TEST_ASSERT_EQUAL(State::CheckElectrodes, m.handle(Event::Start));
}

static void test_electrodes_ok_goes_to_calibrate() {
    TofStateMachine m;
    m.handle(Event::Start);
    TEST_ASSERT_EQUAL(State::Calibrate, m.handle(Event::ElectrodesOk));
}

static void test_electrodes_bad_faults_with_impedance_cause() {
    TofStateMachine m;
    m.handle(Event::Start);
    TEST_ASSERT_EQUAL(State::Fault, m.handle(Event::ElectrodesBad));
    TEST_ASSERT_EQUAL(FaultCause::Impedance, m.fault().cause());
}

static void test_calibration_ok_goes_to_ready() {
    TofStateMachine m;
    m.handle(Event::Start);
    m.handle(Event::ElectrodesOk);
    TEST_ASSERT_EQUAL(State::Ready, m.handle(Event::CalibrationOk));
}

static void test_calibration_failed_faults_with_given_cause() {
    TofStateMachine m;
    m.handle(Event::Start);
    m.handle(Event::ElectrodesOk);
    TEST_ASSERT_EQUAL(State::Fault,
                      m.handle(Event::CalibrationFailed, FaultCause::CalibrationNoPlateau));
    TEST_ASSERT_EQUAL(FaultCause::CalibrationNoPlateau, m.fault().cause());
}

// --- the train ---

static void test_full_train_walks_t1_to_t4_then_waits() {
    TofStateMachine m = ready_machine();
    TEST_ASSERT_EQUAL(State::StimT1, m.handle(Event::Start));
    TEST_ASSERT_EQUAL(State::StimT2, m.handle(Event::PulseComplete));
    TEST_ASSERT_EQUAL(State::StimT3, m.handle(Event::PulseComplete));
    TEST_ASSERT_EQUAL(State::StimT4, m.handle(Event::PulseComplete));
    TEST_ASSERT_EQUAL(State::WaitForResponses, m.handle(Event::PulseComplete));
    TEST_ASSERT_EQUAL_INT(4, m.pulses_delivered());
}

static void test_exactly_four_pulses_are_counted() {
    TofStateMachine m = ready_machine();
    m.handle(Event::Start);
    for (int i = 0; i < 4; ++i) m.handle(Event::PulseComplete);
    // A fifth pulse event in WaitForResponses must not advance the count.
    m.handle(Event::PulseComplete);
    TEST_ASSERT_EQUAL_INT(4, m.pulses_delivered());
    TEST_ASSERT_EQUAL(State::WaitForResponses, m.state());
}

static void test_measurement_completes_back_to_ready() {
    TofStateMachine m = machine_in(State::WaitForResponses);
    TEST_ASSERT_EQUAL(State::Calculate, m.handle(Event::ResponsesReady));
    TEST_ASSERT_EQUAL(State::Display, m.handle(Event::ComputeDone));
    TEST_ASSERT_EQUAL(State::Ready, m.handle(Event::DisplayDone));
}

// --- fault, from everywhere ---

static void test_fault_reachable_from_every_state() {
    for (State s : kAllStates) {
        TofStateMachine m = machine_in(s);
        m.handle(Event::FaultRaised, FaultCause::Stimulator);
        TEST_ASSERT_EQUAL(State::Fault, m.state());
        TEST_ASSERT_TRUE(m.fault().active());
    }
}

static void test_fault_is_latched_against_every_other_event() {
    const Event others[] = {Event::Start,        Event::Stop,
                            Event::ElectrodesOk, Event::CalibrationOk,
                            Event::PulseComplete, Event::ResponsesReady,
                            Event::ComputeDone,  Event::DisplayDone};
    for (Event e : others) {
        TofStateMachine m = machine_in(State::Ready);
        m.handle(Event::FaultRaised, FaultCause::EmgHardware);
        m.handle(e);
        TEST_ASSERT_EQUAL(State::Fault, m.state());
    }
}

static void test_only_acknowledge_leaves_fault() {
    TofStateMachine m = machine_in(State::Ready);
    m.handle(Event::FaultRaised, FaultCause::Stimulator);
    TEST_ASSERT_EQUAL(State::Idle, m.handle(Event::FaultAcknowledged));
    TEST_ASSERT_FALSE(m.fault().active());
    TEST_ASSERT_EQUAL_INT(0, m.pulses_delivered());
}

static void test_fault_without_cause_records_internal() {
    TofStateMachine m;
    m.handle(Event::FaultRaised);
    TEST_ASSERT_EQUAL(FaultCause::Internal, m.fault().cause());
}

// --- stop ---

static void test_stop_returns_to_idle_from_any_non_fault_state() {
    for (State s : kAllStates) {
        if (s == State::Fault) continue;
        TofStateMachine m = machine_in(s);
        m.handle(Event::Stop);
        TEST_ASSERT_EQUAL(State::Idle, m.state());
    }
}

static void test_stop_mid_train_resets_pulse_count() {
    TofStateMachine m = machine_in(State::StimT3);
    TEST_ASSERT_EQUAL_INT(2, m.pulses_delivered());
    m.handle(Event::Stop);
    TEST_ASSERT_EQUAL_INT(0, m.pulses_delivered());
}

// --- the safety invariant ---

static void test_stimulation_only_allowed_while_calibrating_or_stimulating() {
    TEST_ASSERT_FALSE(safety::stimulation_allowed(State::Idle));
    TEST_ASSERT_FALSE(safety::stimulation_allowed(State::CheckElectrodes));
    TEST_ASSERT_TRUE(safety::stimulation_allowed(State::Calibrate));
    TEST_ASSERT_FALSE(safety::stimulation_allowed(State::Ready));
    TEST_ASSERT_TRUE(safety::stimulation_allowed(State::StimT1));
    TEST_ASSERT_TRUE(safety::stimulation_allowed(State::StimT2));
    TEST_ASSERT_TRUE(safety::stimulation_allowed(State::StimT3));
    TEST_ASSERT_TRUE(safety::stimulation_allowed(State::StimT4));
    TEST_ASSERT_FALSE(safety::stimulation_allowed(State::WaitForResponses));
    TEST_ASSERT_FALSE(safety::stimulation_allowed(State::Calculate));
    TEST_ASSERT_FALSE(safety::stimulation_allowed(State::Display));
    TEST_ASSERT_FALSE(safety::stimulation_allowed(State::Fault));
}

static void test_no_stimulation_or_hv_in_fault() {
    // The single property everything else rests on.
    TEST_ASSERT_FALSE(safety::stimulation_allowed(State::Fault));
    TEST_ASSERT_FALSE(safety::hv_allowed(State::Fault));
}

static void test_hv_is_off_in_idle() {
    TEST_ASSERT_FALSE(safety::hv_allowed(State::Idle));
}

static void test_hv_is_on_wherever_stimulation_is() {
    for (State s : kAllStates) {
        if (safety::stimulation_allowed(s)) {
            TEST_ASSERT_TRUE(safety::hv_allowed(s));
        }
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_starts_in_idle);
    RUN_TEST(test_idle_start_goes_to_check_electrodes);
    RUN_TEST(test_electrodes_ok_goes_to_calibrate);
    RUN_TEST(test_electrodes_bad_faults_with_impedance_cause);
    RUN_TEST(test_calibration_ok_goes_to_ready);
    RUN_TEST(test_calibration_failed_faults_with_given_cause);
    RUN_TEST(test_full_train_walks_t1_to_t4_then_waits);
    RUN_TEST(test_exactly_four_pulses_are_counted);
    RUN_TEST(test_measurement_completes_back_to_ready);
    RUN_TEST(test_fault_reachable_from_every_state);
    RUN_TEST(test_fault_is_latched_against_every_other_event);
    RUN_TEST(test_only_acknowledge_leaves_fault);
    RUN_TEST(test_fault_without_cause_records_internal);
    RUN_TEST(test_stop_returns_to_idle_from_any_non_fault_state);
    RUN_TEST(test_stop_mid_train_resets_pulse_count);
    RUN_TEST(test_stimulation_only_allowed_while_calibrating_or_stimulating);
    RUN_TEST(test_no_stimulation_or_hv_in_fault);
    RUN_TEST(test_hv_is_off_in_idle);
    RUN_TEST(test_hv_is_on_wherever_stimulation_is);
    return UNITY_END();
}
