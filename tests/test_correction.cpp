// Tests for correction::apply -- ADR-0006, power law.

#include <cmath>
#include <limits>

#include "correction/correction.hpp"
#include "unity.h"

void setUp() {}
void tearDown() {}

// --- the endpoint property the whole design rests on ---

static void test_full_recovery_maps_to_one_at_every_exponent() {
    // If f(1) != 1 the 0.90 threshold becomes unreachable and the device can
    // never indicate recovery. This is the property a plain multiplier broke.
    const float exps[] = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f};
    for (float e : exps) {
        const correction::Result r = correction::apply(1.0f, e);
        TEST_ASSERT_EQUAL(correction::Status::Ok, r.status);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, r.peripheral_ratio);
    }
}

static void test_threshold_is_reachable_below_full_recovery() {
    // There must exist a facial ratio strictly under 1.0 that certifies --
    // otherwise only a perfectly recovered patient could ever be extubated.
    const float needed = correction::facial_needed_for(0.90f, 2.0f);
    TEST_ASSERT_TRUE(needed < 1.0f);
    TEST_ASSERT_TRUE(needed > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.94868f, needed);

    const correction::Result r = correction::apply(needed, 2.0f);
    TEST_ASSERT_EQUAL(correction::Status::Ok, r.status);
    TEST_ASSERT_TRUE(r.peripheral_ratio >= 0.90f - 1e-5f);
}

static void test_zero_maps_to_zero() {
    const correction::Result r = correction::apply(0.0f, 2.0f);
    TEST_ASSERT_EQUAL(correction::Status::Ok, r.status);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, r.peripheral_ratio);
}

// --- the safety property ---

static void test_correction_never_raises_the_reading() {
    // For every valid ratio and exponent, peripheral <= facial. The facial site
    // reads optimistically, so a correction may only move the value down.
    const float exps[] = {1.0f, 1.3f, 2.0f, 2.7f, 4.0f};
    for (float e : exps) {
        for (float f = 0.0f; f <= 1.0f; f += 0.05f) {
            const correction::Result r = correction::apply(f, e);
            TEST_ASSERT_EQUAL(correction::Status::Ok, r.status);
            TEST_ASSERT_TRUE(r.peripheral_ratio <= f + 1e-5f);
        }
    }
}

static void test_larger_exponent_is_more_conservative() {
    const correction::Result mild = correction::apply(0.90f, 1.5f);
    const correction::Result firm = correction::apply(0.90f, 3.0f);
    TEST_ASSERT_TRUE(firm.peripheral_ratio < mild.peripheral_ratio);
}

static void test_exponent_of_one_is_identity() {
    const correction::Result r = correction::apply(0.87f, 1.0f);
    TEST_ASSERT_EQUAL(correction::Status::Ok, r.status);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.87f, r.peripheral_ratio);
}

static void test_known_values_at_exponent_two() {
    const correction::Result a = correction::apply(0.90f, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.81f, a.peripheral_ratio);
    const correction::Result b = correction::apply(0.95f, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.9025f, b.peripheral_ratio);
}

// --- rejected inputs ---

static void test_ratio_above_one_is_anomalous_not_corrected() {
    // Post-tetanic facilitation or noise. Raising it would read as full
    // recovery on a suspect measurement.
    const correction::Result r = correction::apply(1.12f, 2.0f);
    TEST_ASSERT_EQUAL(correction::Status::AnomalousInput, r.status);
    TEST_ASSERT_TRUE(std::isnan(r.peripheral_ratio));
}

static void test_exponent_below_one_is_rejected() {
    // Would raise the reading -- the unsafe direction.
    const correction::Result r = correction::apply(0.87f, 0.8f);
    TEST_ASSERT_EQUAL(correction::Status::InvalidExponent, r.status);
}

static void test_exponent_above_maximum_is_rejected() {
    const correction::Result r = correction::apply(0.87f, 4.5f);
    TEST_ASSERT_EQUAL(correction::Status::InvalidExponent, r.status);
}

static void test_bounds_are_inclusive() {
    TEST_ASSERT_TRUE(correction::exponent_in_range(correction::kExponentMin));
    TEST_ASSERT_TRUE(correction::exponent_in_range(correction::kExponentMax));
    TEST_ASSERT_FALSE(correction::exponent_in_range(correction::kExponentMin - 0.01f));
    TEST_ASSERT_FALSE(correction::exponent_in_range(correction::kExponentMax + 0.01f));
}

static void test_unconfigured_exponent_reports_not_configured() {
    const correction::Result r =
        correction::apply(0.87f, std::numeric_limits<float>::quiet_NaN());
    TEST_ASSERT_EQUAL(correction::Status::NotConfigured, r.status);
}

static void test_invalid_facial_ratio_is_rejected() {
    const correction::Result nan_in =
        correction::apply(std::numeric_limits<float>::quiet_NaN(), 2.0f);
    TEST_ASSERT_EQUAL(correction::Status::InvalidInput, nan_in.status);
    const correction::Result neg_in = correction::apply(-0.1f, 2.0f);
    TEST_ASSERT_EQUAL(correction::Status::InvalidInput, neg_in.status);
}

static void test_facial_needed_rejects_bad_exponent() {
    TEST_ASSERT_TRUE(std::isnan(correction::facial_needed_for(0.90f, 0.5f)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_recovery_maps_to_one_at_every_exponent);
    RUN_TEST(test_threshold_is_reachable_below_full_recovery);
    RUN_TEST(test_zero_maps_to_zero);
    RUN_TEST(test_correction_never_raises_the_reading);
    RUN_TEST(test_larger_exponent_is_more_conservative);
    RUN_TEST(test_exponent_of_one_is_identity);
    RUN_TEST(test_known_values_at_exponent_two);
    RUN_TEST(test_ratio_above_one_is_anomalous_not_corrected);
    RUN_TEST(test_exponent_below_one_is_rejected);
    RUN_TEST(test_exponent_above_maximum_is_rejected);
    RUN_TEST(test_bounds_are_inclusive);
    RUN_TEST(test_unconfigured_exponent_reports_not_configured);
    RUN_TEST(test_invalid_facial_ratio_is_rejected);
    RUN_TEST(test_facial_needed_rejects_bad_exponent);
    return UNITY_END();
}
