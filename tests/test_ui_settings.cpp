// Tests for typed entry, setting bounds, navigation and menu tiles.

#include <cstring>

#include "correction/correction.hpp"
#include "ui/menu_model.hpp"
#include "ui/settings.hpp"
#include "unity.h"

void setUp() {}
void tearDown() {}

namespace {

ui::Entry typed(ui::Field f, const char* keys) {
    ui::Entry e;
    e.field = f;
    for (const char* k = keys; *k != '\0'; ++k) {
        if (*k == '.') {
            ui::press_point(e);
        } else if (*k == '<') {
            ui::press_delete(e);
        } else {
            ui::press_digit(e, *k);
        }
    }
    return e;
}

}  // namespace

// --- bounds ---

static void test_exponent_bounds_match_correction() {
    // The keypad must not accept an exponent correction::apply would reject.
    TEST_ASSERT_EQUAL_FLOAT(correction::kExponentMin, ui::spec(ui::Field::Exponent).min);
    TEST_ASSERT_EQUAL_FLOAT(correction::kExponentMax, ui::spec(ui::Field::Exponent).max);
}

static void test_bounds_are_inclusive() {
    TEST_ASSERT_TRUE(ui::in_range(ui::Field::PulseWidth, 200.0f));
    TEST_ASSERT_TRUE(ui::in_range(ui::Field::PulseWidth, 300.0f));
    TEST_ASSERT_FALSE(ui::in_range(ui::Field::PulseWidth, 199.0f));
    TEST_ASSERT_FALSE(ui::in_range(ui::Field::PulseWidth, 301.0f));
    // A typed "0.6" parses to slightly more than 0.6f's neighbour; still in.
    TEST_ASSERT_TRUE(ui::in_range(ui::Field::Theta, 0.6f));
}

// --- keypad editing ---

static void test_typing_builds_the_number() {
    TEST_ASSERT_EQUAL_STRING("2.5", typed(ui::Field::Exponent, "2.5").buf);
}

static void test_leading_zero_is_replaced() {
    TEST_ASSERT_EQUAL_STRING("7", typed(ui::Field::Interval, "07").buf);
}

static void test_point_first_gives_leading_zero() {
    TEST_ASSERT_EQUAL_STRING("0.", typed(ui::Field::Theta, ".").buf);
}

static void test_second_point_is_ignored() {
    TEST_ASSERT_EQUAL_STRING("1.2", typed(ui::Field::Theta, "1..2").buf);
}

static void test_point_is_ignored_on_whole_number_field() {
    TEST_ASSERT_EQUAL_STRING("25", typed(ui::Field::Interval, "2.5").buf);
}

static void test_delete_removes_last_character() {
    TEST_ASSERT_EQUAL_STRING("2", typed(ui::Field::Exponent, "2.<").buf);
    TEST_ASSERT_EQUAL_STRING("", typed(ui::Field::Exponent, "<").buf);
}

static void test_length_is_capped() {
    TEST_ASSERT_EQUAL_STRING("123456", typed(ui::Field::Interval, "1234567").buf);
}

// --- validation ---

static void test_empty_entry_cannot_be_entered() {
    const ui::EntryCheck c = ui::check(typed(ui::Field::Exponent, ""));
    TEST_ASSERT_FALSE(c.valid);
    TEST_ASSERT_FALSE(c.error);
    TEST_ASSERT_EQUAL_STRING("Allowed 1.0 \xE2\x80\x93 4.0", c.message);
}

static void test_value_in_range_is_valid() {
    const ui::EntryCheck c = ui::check(typed(ui::Field::Exponent, "2.5"));
    TEST_ASSERT_TRUE(c.valid);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, c.value);
}

static void test_value_out_of_range_is_an_error() {
    const ui::EntryCheck c = ui::check(typed(ui::Field::Exponent, "5"));
    TEST_ASSERT_FALSE(c.valid);
    TEST_ASSERT_TRUE(c.error);
    TEST_ASSERT_EQUAL_STRING("Outside 1.0 \xE2\x80\x93 4.0", c.message);
}

static void test_too_many_decimals_is_an_error() {
    const ui::EntryCheck c = ui::check(typed(ui::Field::Exponent, "2.55"));
    TEST_ASSERT_FALSE(c.valid);
    TEST_ASSERT_EQUAL_STRING("At most 1 decimal places", c.message);
}

static void test_trailing_point_is_not_yet_an_error() {
    // "0." on the way to "0.5" should not flash red.
    const ui::EntryCheck c = ui::check(typed(ui::Field::Exponent, "0."));
    TEST_ASSERT_FALSE(c.valid);
    TEST_ASSERT_FALSE(c.error);
}

// --- navigation ---

static void test_back_goes_up_the_tree() {
    TEST_ASSERT_EQUAL(ui::Page::Menu, ui::parent_of(ui::Page::Measurement));
    TEST_ASSERT_EQUAL(ui::Page::Device, ui::parent_of(ui::Page::Brightness));
    TEST_ASSERT_EQUAL(ui::Page::Alarms, ui::parent_of(ui::Page::Choice));
    TEST_ASSERT_EQUAL(ui::Page::Monitor, ui::parent_of(ui::Page::Menu));
}

// --- tiles ---

static void test_menu_has_six_tiles_that_open_pages() {
    ui::Tile t[ui::kMaxTiles];
    const int n = ui::tiles_for(ui::Page::Menu, ui::Snapshot{}, t);
    TEST_ASSERT_EQUAL(6, n);
    for (int i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL(ui::TileTarget::Kind::OpenPage, t[i].target.kind);
    }
    TEST_ASSERT_EQUAL(ui::Page::Signal, t[5].target.page);
}

static void test_measurement_tiles_show_current_values() {
    ui::Snapshot s;
    s.settings.interval_s = 20;
    s.settings.exponent = 2.0f;
    s.settings.theta_mv = 0.1f;
    ui::Tile t[ui::kMaxTiles];
    TEST_ASSERT_EQUAL(3, ui::tiles_for(ui::Page::Measurement, s, t));
    TEST_ASSERT_EQUAL_STRING("20 s", t[0].value);
    TEST_ASSERT_EQUAL_STRING("n 2.0", t[1].value);
    TEST_ASSERT_EQUAL_STRING("0.10 mV", t[2].value);
    TEST_ASSERT_EQUAL(ui::TileTarget::Kind::OpenEntry, t[1].target.kind);
    TEST_ASSERT_EQUAL(ui::Field::Exponent, t[1].target.field);
}

static void test_operating_current_is_read_only() {
    ui::Snapshot s;
    ui::Tile t[ui::kMaxTiles];
    ui::tiles_for(ui::Page::Stimulation, s, t);
    TEST_ASSERT_FALSE(t[1].editable);
    TEST_ASSERT_EQUAL_STRING("Not calibrated", t[1].value);
    s.operating_current_ma = 52.0f;
    ui::tiles_for(ui::Page::Stimulation, s, t);
    TEST_ASSERT_EQUAL_STRING("52.0 mA", t[1].value);
}

// --- fault ---

static void test_impedance_fault_reports_measured_value() {
    const ui::FaultText f = ui::present_fault(safety::FaultCause::Impedance, 62000.0f);
    TEST_ASSERT_EQUAL_STRING("Electrode impedance out of range", f.cause);
    TEST_ASSERT_NOT_NULL(std::strstr(f.detail, "62.0 k"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_exponent_bounds_match_correction);
    RUN_TEST(test_bounds_are_inclusive);
    RUN_TEST(test_typing_builds_the_number);
    RUN_TEST(test_leading_zero_is_replaced);
    RUN_TEST(test_point_first_gives_leading_zero);
    RUN_TEST(test_second_point_is_ignored);
    RUN_TEST(test_point_is_ignored_on_whole_number_field);
    RUN_TEST(test_delete_removes_last_character);
    RUN_TEST(test_length_is_capped);
    RUN_TEST(test_empty_entry_cannot_be_entered);
    RUN_TEST(test_value_in_range_is_valid);
    RUN_TEST(test_value_out_of_range_is_an_error);
    RUN_TEST(test_too_many_decimals_is_an_error);
    RUN_TEST(test_trailing_point_is_not_yet_an_error);
    RUN_TEST(test_back_goes_up_the_tree);
    RUN_TEST(test_menu_has_six_tiles_that_open_pages);
    RUN_TEST(test_measurement_tiles_show_current_values);
    RUN_TEST(test_operating_current_is_read_only);
    RUN_TEST(test_impedance_fault_reports_measured_value);
    return UNITY_END();
}
