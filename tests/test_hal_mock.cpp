// Tests for the fakes. They enforce the same bounds as the real hardware, so a
// test that passes against them is not passing on a permissive stub.

#include "hal/impedance.hpp"
#include "hal_mock/cmap_source.hpp"
#include "hal_mock/mock_stimulator.hpp"
#include "tof/tof_compute.hpp"
#include "unity.h"

void setUp() {}
void tearDown() {}

// --- stimulator bounds ---

static void test_pulse_requires_output_enabled() {
    hal_mock::MockStimulator s;
    s.set_amplitude_ma(20.0f);
    TEST_ASSERT_FALSE(s.fire_pulse(250));
    TEST_ASSERT_EQUAL_size_t(0, s.pulses().size());
}

static void test_pulse_is_recorded_with_amplitude_and_width() {
    hal_mock::MockStimulator s;
    s.enable_output(true);
    s.set_amplitude_ma(30.0f);
    TEST_ASSERT_TRUE(s.fire_pulse(250));
    TEST_ASSERT_EQUAL_size_t(1, s.pulses().size());
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 30.0f, s.pulses()[0].amplitude_ma);
    TEST_ASSERT_EQUAL_UINT32(250, s.pulses()[0].width_us);
}

static void test_amplitude_above_thirty_milliamps_is_rejected() {
    hal_mock::MockStimulator s;
    TEST_ASSERT_FALSE(s.set_amplitude_ma(30.1f));
    TEST_ASSERT_TRUE(s.set_amplitude_ma(30.0f));
}

static void test_pulse_width_outside_spec_range_is_rejected() {
    hal_mock::MockStimulator s;
    s.enable_output(true);
    s.set_amplitude_ma(20.0f);
    TEST_ASSERT_FALSE(s.fire_pulse(199));
    TEST_ASSERT_FALSE(s.fire_pulse(301));
    TEST_ASSERT_TRUE(s.fire_pulse(200));
    TEST_ASSERT_TRUE(s.fire_pulse(300));
}

static void test_faulted_stimulator_refuses_everything() {
    hal_mock::MockStimulator s;
    s.enable_output(true);
    s.set_faulted(true);
    TEST_ASSERT_FALSE(s.set_amplitude_ma(20.0f));
    TEST_ASSERT_FALSE(s.fire_pulse(250));
    TEST_ASSERT_FALSE(s.enable_output(true));
}

// --- synthetic CMAP ---

static void test_no_blockade_gives_flat_train_and_ratio_one() {
    hal_mock::CmapParams p;
    p.fade = 1.0f;
    float a[tof::kTwitchCount];
    hal_mock::generate_train(p, a);

    const tof::Result r = tof::compute(a, 0.1f);
    TEST_ASSERT_EQUAL_UINT8(4, r.count);
    TEST_ASSERT_TRUE(r.ratio_valid);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, r.ratio);
}

static void test_fade_produces_expected_ratio() {
    hal_mock::CmapParams p;
    p.fade = 0.9f;  // ratio = 0.9^3
    float a[tof::kTwitchCount];
    hal_mock::generate_train(p, a);

    const tof::Result r = tof::compute(a, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.729f, r.ratio);
}

static void test_twitches_drop_out_in_reverse_order() {
    // Deepening blockade removes T4 first, then T3 -- the order that makes a
    // gapped pattern non-physiological.
    hal_mock::CmapParams p;
    p.t1_amplitude_mv = 2.0f;
    p.fade = 0.5f;       // 2.0, 1.0, 0.5, 0.25
    p.dropout_mv = 0.4f;  // removes T4 only

    float a[tof::kTwitchCount];
    hal_mock::generate_train(p, a);
    const tof::Result r = tof::compute(a, 0.1f);

    TEST_ASSERT_EQUAL_UINT8(3, r.count);
    TEST_ASSERT_TRUE(r.detected[2]);
    TEST_ASSERT_FALSE(r.detected[3]);
    TEST_ASSERT_FALSE(r.ratio_valid);
}

static void test_recruitment_curve_saturates_at_supramaximal() {
    hal_mock::CmapParams p;
    p.supramaximal_ma = 30.0f;
    p.t1_amplitude_mv = 2.0f;

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, hal_mock::response_at(p, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, hal_mock::response_at(p, 15.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.0f, hal_mock::response_at(p, 30.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.0f, hal_mock::response_at(p, 60.0f));
}

// --- impedance window ---

static void test_impedance_window_matches_spec() {
    TEST_ASSERT_FALSE(hal::impedance_in_range(499.0f));
    TEST_ASSERT_TRUE(hal::impedance_in_range(500.0f));
    TEST_ASSERT_TRUE(hal::impedance_in_range(50000.0f));
    TEST_ASSERT_FALSE(hal::impedance_in_range(50001.0f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pulse_requires_output_enabled);
    RUN_TEST(test_pulse_is_recorded_with_amplitude_and_width);
    RUN_TEST(test_amplitude_above_thirty_milliamps_is_rejected);
    RUN_TEST(test_pulse_width_outside_spec_range_is_rejected);
    RUN_TEST(test_faulted_stimulator_refuses_everything);
    RUN_TEST(test_no_blockade_gives_flat_train_and_ratio_one);
    RUN_TEST(test_fade_produces_expected_ratio);
    RUN_TEST(test_twitches_drop_out_in_reverse_order);
    RUN_TEST(test_recruitment_curve_saturates_at_supramaximal);
    RUN_TEST(test_impedance_window_matches_spec);
    return UNITY_END();
}
