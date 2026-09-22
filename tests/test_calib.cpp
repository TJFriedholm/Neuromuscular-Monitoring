// Tests for calib::run -- Algorithm 2 as corrected by ADR-0003.

#include <vector>

#include "calib/calib.hpp"
#include "hal_mock/cmap_source.hpp"
#include "unity.h"

void setUp() {}
void tearDown() {}

namespace {

// Records every current the ramp asked for, and answers from a recruitment
// curve. This is the seam that makes Algorithm 2 testable at all.
struct Probe {
    hal_mock::CmapParams params;
    std::vector<float> currents;
    // When set, every response is this value regardless of current -- used to
    // simulate a lead off, where nothing is evoked at any amplitude.
    bool flat = false;
    float flat_value = 0.0f;
};

float measure(float current_ma, void* ctx) {
    auto* p = static_cast<Probe*>(ctx);
    p->currents.push_back(current_ma);
    if (p->flat) {
        return p->flat_value;
    }
    return hal_mock::response_at(p->params, current_ma);
}

calib::Config base_config() {
    calib::Config c;
    c.i_start_ma = 10.0f;
    c.i_step_ma = 5.0f;
    c.i_max_ma = 60.0f;
    c.theta_mv = 0.10f;
    c.delta_mv = 0.05f;
    return c;
}

}  // namespace

// --- the bug ADR-0003 exists to fix ---

static void test_subthreshold_start_is_not_mistaken_for_plateau() {
    // Algorithm 2 as written breaks on iteration one: R_prev is 0, so the
    // plateau test reduces to "is the response small?" -- which is true
    // precisely because 10 mA is below threshold. It would return Ok at 13 mA
    // having never evoked a CMAP.
    Probe p;
    p.params.supramaximal_ma = 40.0f;
    p.params.t1_amplitude_mv = 2.0f;

    const calib::Result r = calib::run(base_config(), 5000.0f, nullptr, 0, measure, &p);

    TEST_ASSERT_EQUAL(calib::Status::Ok, r.status);
    // Must have kept ramping past the subthreshold start.
    TEST_ASSERT_TRUE(p.currents.size() > 2);
    TEST_ASSERT_TRUE(r.i_cal_ma > 13.0f);
}

static void test_no_response_at_any_current_reports_no_response() {
    // Lead off: nothing is evoked anywhere on the ramp.
    Probe p;
    p.flat = true;
    p.flat_value = 0.01f;

    const calib::Result r = calib::run(base_config(), 5000.0f, nullptr, 0, measure, &p);

    TEST_ASSERT_EQUAL(calib::Status::NoResponse, r.status);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.i_cal_ma);
}

static void test_exhausting_the_ramp_is_not_success() {
    // Responses grow but never plateau below i_max. Algorithm 2 returned
    // i_cal = i_max here, indistinguishable from a real result, leaving the
    // device at maximum current on a failed calibration.
    Probe p;
    p.params.supramaximal_ma = 500.0f;  // never reached within the ramp
    p.params.t1_amplitude_mv = 20.0f;

    const calib::Result r = calib::run(base_config(), 5000.0f, nullptr, 0, measure, &p);

    TEST_ASSERT_EQUAL(calib::Status::NoPlateau, r.status);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.i_cal_ma);
}

// --- normal operation ---

static void test_plateau_applies_thirty_percent_margin() {
    Probe p;
    p.params.supramaximal_ma = 30.0f;
    p.params.t1_amplitude_mv = 2.0f;

    const calib::Result r = calib::run(base_config(), 5000.0f, nullptr, 0, measure, &p);

    TEST_ASSERT_EQUAL(calib::Status::Ok, r.status);
    // Plateau is first seen at 35 mA (30 and 35 both give full amplitude).
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.3f * 35.0f, r.i_cal_ma);
}

static void test_margin_is_clamped_to_i_max() {
    Probe p;
    p.params.supramaximal_ma = 15.0f;
    p.params.t1_amplitude_mv = 2.0f;

    calib::Config c = base_config();
    c.i_max_ma = 22.0f;  // 1.3 * 20 = 26 > 22

    const calib::Result r = calib::run(c, 5000.0f, nullptr, 0, measure, &p);

    TEST_ASSERT_EQUAL(calib::Status::Ok, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.0f, r.i_cal_ma);
}

static void test_ramp_visits_expected_currents() {
    Probe p;
    p.params.supramaximal_ma = 500.0f;
    p.params.t1_amplitude_mv = 20.0f;

    calib::run(base_config(), 5000.0f, nullptr, 0, measure, &p);

    TEST_ASSERT_EQUAL_size_t(11, p.currents.size());  // 10,15,...,60
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, p.currents.front());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, p.currents.back());
}

// --- impedance gate ---

static void test_impedance_below_range_rejects_before_stimulating() {
    Probe p;
    const calib::Result r = calib::run(base_config(), 400.0f, nullptr, 0, measure, &p);
    TEST_ASSERT_EQUAL(calib::Status::BadImpedance, r.status);
    TEST_ASSERT_EQUAL_size_t(0, p.currents.size());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.i_cal_ma);
}

static void test_impedance_above_range_rejects_before_stimulating() {
    Probe p;
    const calib::Result r = calib::run(base_config(), 60000.0f, nullptr, 0, measure, &p);
    TEST_ASSERT_EQUAL(calib::Status::BadImpedance, r.status);
    TEST_ASSERT_EQUAL_size_t(0, p.currents.size());
}

// --- baseline ---

static void test_baseline_is_mean_of_samples() {
    Probe p;
    p.params.supramaximal_ma = 30.0f;
    const float samples[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    const calib::Result r = calib::run(base_config(), 5000.0f, samples, 5, measure, &p);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.3f, r.baseline_mv);
}

static void test_baseline_without_samples_is_zero() {
    Probe p;
    p.params.supramaximal_ma = 30.0f;
    const calib::Result r = calib::run(base_config(), 5000.0f, nullptr, 0, measure, &p);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.baseline_mv);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_subthreshold_start_is_not_mistaken_for_plateau);
    RUN_TEST(test_no_response_at_any_current_reports_no_response);
    RUN_TEST(test_exhausting_the_ramp_is_not_success);
    RUN_TEST(test_plateau_applies_thirty_percent_margin);
    RUN_TEST(test_margin_is_clamped_to_i_max);
    RUN_TEST(test_ramp_visits_expected_currents);
    RUN_TEST(test_impedance_below_range_rejects_before_stimulating);
    RUN_TEST(test_impedance_above_range_rejects_before_stimulating);
    RUN_TEST(test_baseline_is_mean_of_samples);
    RUN_TEST(test_baseline_without_samples_is_zero);
    return UNITY_END();
}
