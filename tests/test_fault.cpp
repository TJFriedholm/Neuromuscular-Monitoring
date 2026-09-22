// Tests for safety::FaultLatch -- ADR-0005.

#include "safety/fault.hpp"
#include "unity.h"

void setUp() {}
void tearDown() {}

static void test_starts_inactive() {
    safety::FaultLatch f;
    TEST_ASSERT_FALSE(f.active());
    TEST_ASSERT_EQUAL(safety::FaultCause::None, f.cause());
}

static void test_raise_latches_with_cause() {
    safety::FaultLatch f;
    f.raise(safety::FaultCause::Impedance);
    TEST_ASSERT_TRUE(f.active());
    TEST_ASSERT_EQUAL(safety::FaultCause::Impedance, f.cause());
}

static void test_first_cause_is_retained() {
    // Later faults are usually consequences of the first; the root cause is
    // the useful one to show.
    safety::FaultLatch f;
    f.raise(safety::FaultCause::Impedance);
    f.raise(safety::FaultCause::EmgHardware);
    TEST_ASSERT_EQUAL(safety::FaultCause::Impedance, f.cause());
}

static void test_raising_none_does_nothing() {
    safety::FaultLatch f;
    f.raise(safety::FaultCause::None);
    TEST_ASSERT_FALSE(f.active());
}

static void test_stays_latched_until_acknowledged() {
    // The reason latching exists: an electrode drifting across the impedance
    // limit must not make the device resume stimulating on its own.
    safety::FaultLatch f;
    f.raise(safety::FaultCause::Impedance);
    TEST_ASSERT_TRUE(f.active());
    TEST_ASSERT_TRUE(f.active());  // no self-clearing
    f.acknowledge();
    TEST_ASSERT_FALSE(f.active());
    TEST_ASSERT_EQUAL(safety::FaultCause::None, f.cause());
}

static void test_can_refault_after_acknowledge() {
    safety::FaultLatch f;
    f.raise(safety::FaultCause::Stimulator);
    f.acknowledge();
    f.raise(safety::FaultCause::EmgHardware);
    TEST_ASSERT_TRUE(f.active());
    TEST_ASSERT_EQUAL(safety::FaultCause::EmgHardware, f.cause());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_starts_inactive);
    RUN_TEST(test_raise_latches_with_cause);
    RUN_TEST(test_first_cause_is_retained);
    RUN_TEST(test_raising_none_does_nothing);
    RUN_TEST(test_stays_latched_until_acknowledged);
    RUN_TEST(test_can_refault_after_acknowledge);
    return UNITY_END();
}
