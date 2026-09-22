// Tests for tof::compute -- Algorithm 1 as corrected by ADR-0004.

#include <cmath>

#include "tof/tof_compute.hpp"
#include "unity.h"

void setUp() {}
void tearDown() {}

static constexpr float kTheta = 0.10f;  // mV

// --- counting (any-of-four, per Algorithm 1) ---

static void test_all_four_detected_counts_four() {
    const float a[4] = {2.0f, 1.8f, 1.6f, 1.4f};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_EQUAL_UINT8(4, r.count);
}

static void test_no_twitches_above_threshold_counts_zero() {
    const float a[4] = {0.05f, 0.02f, 0.0f, 0.0f};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_EQUAL_UINT8(0, r.count);
    TEST_ASSERT_FALSE(r.ratio_valid);
}

static void test_fade_dropout_counts_remaining() {
    // T4 and T3 gone -- the order real blockade removes them in.
    const float a[4] = {2.0f, 1.2f, 0.0f, 0.0f};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_EQUAL_UINT8(2, r.count);
}

static void test_gap_counts_all_detectable_not_the_prefix() {
    // Physiologically impossible, but Algorithm 1 counts position-independently
    // and this pins that behaviour down. A prefix rule would report 1.
    const float a[4] = {2.0f, 0.0f, 1.5f, 1.2f};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_EQUAL_UINT8(3, r.count);
}

static void test_threshold_is_strict() {
    const float a[4] = {kTheta, kTheta, kTheta, kTheta};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_EQUAL_UINT8(0, r.count);
}

// --- ratio ---

static void test_ratio_is_t4_over_t1() {
    const float a[4] = {2.0f, 1.8f, 1.6f, 1.0f};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_TRUE(r.ratio_valid);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, r.ratio);
}

static void test_ratio_needs_only_t1_and_t4() {
    // Algorithm 1 gates on T1 and T4 alone, not on all four.
    const float a[4] = {2.0f, 0.0f, 0.0f, 1.0f};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_TRUE(r.ratio_valid);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, r.ratio);
}

static void test_missing_t4_invalidates_ratio() {
    const float a[4] = {2.0f, 1.5f, 1.0f, 0.0f};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_FALSE(r.ratio_valid);
    TEST_ASSERT_TRUE(std::isnan(r.ratio));
}

static void test_missing_t1_invalidates_ratio() {
    const float a[4] = {0.0f, 1.5f, 1.0f, 0.8f};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_FALSE(r.ratio_valid);
    TEST_ASSERT_TRUE(std::isnan(r.ratio));
}

// --- ADR-0004: validity is separate from value ---

static void test_profound_block_ratio_near_zero_is_still_valid() {
    // The case the spec's `R_TOF <- 0` sentinel destroys: a real, very small
    // ratio must be reported as a measurement, not as "unavailable".
    const float a[4] = {2.0f, 0.5f, 0.2f, 0.102f};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_TRUE(r.ratio_valid);
    TEST_ASSERT_TRUE(r.ratio > 0.0f);
    TEST_ASSERT_TRUE(r.ratio < 0.1f);
}

static void test_ratio_above_one_is_not_clamped() {
    // Post-tetanic facilitation, or noise near recovery. Clamping this to 1.00
    // would render an anomaly as the most confident possible "fully recovered".
    const float a[4] = {1.0f, 1.1f, 1.2f, 1.4f};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_TRUE(r.ratio_valid);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.4f, r.ratio);
}

static void test_detected_flags_match_amplitudes() {
    const float a[4] = {2.0f, 0.0f, 1.5f, 0.05f};
    const tof::Result r = tof::compute(a, kTheta);
    TEST_ASSERT_TRUE(r.detected[0]);
    TEST_ASSERT_FALSE(r.detected[1]);
    TEST_ASSERT_TRUE(r.detected[2]);
    TEST_ASSERT_FALSE(r.detected[3]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_all_four_detected_counts_four);
    RUN_TEST(test_no_twitches_above_threshold_counts_zero);
    RUN_TEST(test_fade_dropout_counts_remaining);
    RUN_TEST(test_gap_counts_all_detectable_not_the_prefix);
    RUN_TEST(test_threshold_is_strict);
    RUN_TEST(test_ratio_is_t4_over_t1);
    RUN_TEST(test_ratio_needs_only_t1_and_t4);
    RUN_TEST(test_missing_t4_invalidates_ratio);
    RUN_TEST(test_missing_t1_invalidates_ratio);
    RUN_TEST(test_profound_block_ratio_near_zero_is_still_valid);
    RUN_TEST(test_ratio_above_one_is_not_clamped);
    RUN_TEST(test_detected_flags_match_amplitudes);
    return UNITY_END();
}
