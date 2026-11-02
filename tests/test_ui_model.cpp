// Tests for ui::present -- what the Monitor screen shows for a given state.

#include <cmath>
#include <cstring>

#include "ui/monitor_model.hpp"
#include "unity.h"

void setUp() {}
void tearDown() {}

namespace {

// A Train-of-Four already computed by the real core functions, so these tests
// exercise the same inputs the device produces.
ui::MonitorInput measured(float t1, float fade, float exponent = 2.0f) {
    ui::MonitorInput in;
    in.state = safety::State::Ready;
    in.has_measurement = true;
    in.alarm_limit = 0.90f;
    float a = t1;
    for (float& amp : in.amps_mv) {
        amp = a;
        a *= fade;
    }
    in.tof = tof::compute(in.amps_mv, 0.10f);
    in.correction = in.tof.ratio_valid ? correction::apply(in.tof.ratio, exponent)
                                       : correction::Result{correction::Status::InvalidInput, NAN};
    return in;
}

}  // namespace

// --- the ratio card: the screen's primary number ---

static void test_below_alarm_limit_shows_peripheral_in_caution() {
    const ui::MonitorView v = ui::present(measured(2.0f, 0.955f));
    // facial 0.955^3 = 0.871; peripheral 0.871^2 = 0.759
    TEST_ASSERT_EQUAL_STRING("0.76", v.peripheral);
    TEST_ASSERT_EQUAL_STRING("0.87", v.facial);
    TEST_ASSERT_EQUAL(ui::Tone::Caution, v.ratio_tone);
    TEST_ASSERT_EQUAL_STRING("Below 0.90", v.chip);
    TEST_ASSERT_TRUE(v.show_facial);
}

static void test_at_alarm_limit_reads_as_reached() {
    // The limit is inclusive: exactly 0.90 must certify, as in correction.hpp.
    ui::MonitorInput in = measured(2.0f, 0.99f);
    in.correction = correction::Result{correction::Status::Ok, 0.90f};
    const ui::MonitorView v = ui::present(in);
    TEST_ASSERT_EQUAL(ui::Tone::Good, v.ratio_tone);
    TEST_ASSERT_EQUAL_STRING("0.90 reached", v.chip);
}

static void test_ratio_above_one_is_suspect_and_has_no_peripheral() {
    // T4 > T1 is not recovery. Showing a peripheral value would read as "fully
    // recovered" on a suspect measurement.
    const ui::MonitorView v = ui::present(measured(2.0f, 1.02f));
    TEST_ASSERT_EQUAL_STRING("Suspect", v.chip);
    TEST_ASSERT_EQUAL(ui::Tone::Caution, v.ratio_tone);
    TEST_ASSERT_NULL(std::strstr(v.peripheral, "."));  // no number shown
    TEST_ASSERT_TRUE(v.show_facial);                  // the raw facial ratio still is
}

static void test_missing_t4_withholds_ratio_and_points_to_count() {
    const ui::MonitorView v = ui::present(measured(2.0f, 0.2f));  // T3, T4 below θ
    TEST_ASSERT_FALSE(v.show_facial);
    TEST_ASSERT_EQUAL_STRING("No ratio", v.chip);
    TEST_ASSERT_EQUAL_STRING("T4 absent. Follow the count.", v.ratio_note);
    TEST_ASSERT_EQUAL_STRING("2", v.count);
}

static void test_no_response_asks_to_check_placement() {
    const ui::MonitorView v = ui::present(measured(0.0f, 1.0f));
    TEST_ASSERT_EQUAL_STRING("0", v.count);
    TEST_ASSERT_EQUAL_STRING("No response. Check placement.", v.ratio_note);
}

static void test_unconfigured_exponent_shows_no_peripheral() {
    ui::MonitorInput in = measured(2.0f, 0.99f);
    in.correction = correction::Result{correction::Status::NotConfigured, NAN};
    const ui::MonitorView v = ui::present(in);
    TEST_ASSERT_EQUAL_STRING("No exponent", v.chip);
    TEST_ASSERT_NULL(std::strstr(v.peripheral, "."));
}

// --- before any measurement ---

static void test_idle_shows_placeholders_and_enables_start_only() {
    const ui::MonitorView v = ui::present(ui::MonitorInput{});
    TEST_ASSERT_EQUAL_STRING("Idle", v.state);
    TEST_ASSERT_EQUAL_STRING("Stopped", v.next);
    TEST_ASSERT_FALSE(v.show_chip);
    TEST_ASSERT_EQUAL_STRING("No measurement yet", v.ratio_note);
    TEST_ASSERT_EQUAL(ui::Tone::Faint, v.count_tone);
    TEST_ASSERT_TRUE(v.start_enabled);
    TEST_ASSERT_FALSE(v.stop_enabled);
    for (const auto& b : v.bars) {
        TEST_ASSERT_FALSE(b.detected);
    }
}

// --- device state and controls ---

static void test_running_disables_start_and_enables_stop() {
    ui::MonitorInput in;
    in.state = safety::State::StimT2;
    const ui::MonitorView v = ui::present(in);
    TEST_ASSERT_EQUAL_STRING("Monitoring", v.state);
    TEST_ASSERT_EQUAL_STRING("Measuring", v.next);
    TEST_ASSERT_FALSE(v.start_enabled);
    TEST_ASSERT_EQUAL_STRING("Running", v.start_label);
    TEST_ASSERT_TRUE(v.stop_enabled);
}

static void test_calibrating_hides_next_measurement() {
    ui::MonitorInput in;
    in.state = safety::State::Calibrate;
    const ui::MonitorView v = ui::present(in);
    TEST_ASSERT_EQUAL_STRING("Calibrating", v.state);
    TEST_ASSERT_EQUAL_STRING("", v.next);
}

static void test_fault_shows_alert_and_offers_no_stop() {
    // A latched fault is left only by acknowledgement (ADR-0005), so STOP is
    // not offered -- it would look like a way to clear the fault.
    ui::MonitorInput in;
    in.state = safety::State::Fault;
    const ui::MonitorView v = ui::present(in);
    TEST_ASSERT_EQUAL_STRING("Fault", v.state);
    TEST_ASSERT_EQUAL(ui::Tone::Alert, v.state_tone);
    TEST_ASSERT_FALSE(v.stop_enabled);
}

static void test_bad_impedance_is_flagged() {
    ui::MonitorInput in;
    in.impedance_ok = false;
    const ui::MonitorView v = ui::present(in);
    TEST_ASSERT_EQUAL_STRING("Check electrodes", v.electrodes);
    TEST_ASSERT_EQUAL(ui::Tone::Alert, v.electrodes_tone);
}

// --- twitch bars ---

static void test_bars_mark_detection_and_clamp_fill() {
    const ui::MonitorView v = ui::present(measured(3.0f, 0.2f));  // T1 above full scale
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, v.bars[0].fill);
    TEST_ASSERT_TRUE(v.bars[0].detected);
    TEST_ASSERT_FALSE(v.bars[3].detected);
    TEST_ASSERT_TRUE(v.bars[3].fill > 0.0f);  // an empty track still shows a sliver
    TEST_ASSERT_EQUAL_STRING("T4", v.bars[3].name);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_below_alarm_limit_shows_peripheral_in_caution);
    RUN_TEST(test_at_alarm_limit_reads_as_reached);
    RUN_TEST(test_ratio_above_one_is_suspect_and_has_no_peripheral);
    RUN_TEST(test_missing_t4_withholds_ratio_and_points_to_count);
    RUN_TEST(test_no_response_asks_to_check_placement);
    RUN_TEST(test_unconfigured_exponent_shows_no_peripheral);
    RUN_TEST(test_idle_shows_placeholders_and_enables_start_only);
    RUN_TEST(test_running_disables_start_and_enables_stop);
    RUN_TEST(test_calibrating_hides_next_measurement);
    RUN_TEST(test_fault_shows_alert_and_offers_no_stop);
    RUN_TEST(test_bad_impedance_is_flagged);
    RUN_TEST(test_bars_mark_detection_and_clamp_fill);
    return UNITY_END();
}
