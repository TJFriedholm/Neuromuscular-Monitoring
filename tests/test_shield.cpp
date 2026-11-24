// Host tests for the pure parts of the shield drivers: register encoding,
// scaling, the conversion counter and the amplitude table.

#include <cstdint>

#include "shield/ad5241_regs.hpp"
#include "shield/ads122c04_regs.hpp"
#include "shield/amplitude_table.hpp"
#include "shield/board.hpp"
#include "shield/sample_math.hpp"
#include "hal/stimulator.hpp"
#include "unity.h"

using namespace shield;
using namespace shield::ads122c04;

void setUp() {}
void tearDown() {}

// --- ADS122C04 commands and registers ---

static void test_register_commands_carry_the_register_number() {
    TEST_ASSERT_EQUAL_HEX8(0x20, cmd_rreg(0));
    TEST_ASSERT_EQUAL_HEX8(0x28, cmd_rreg(2));
    TEST_ASSERT_EQUAL_HEX8(0x44, cmd_wreg(1));
    TEST_ASSERT_EQUAL_HEX8(0x4C, cmd_wreg(3));
}

static void test_emg_sampling_config_encodes_as_expected() {
    // AIN0-AIN1 differential, gain 1, PGA bypassed, 1000 SPS x turbo =
    // 2000 SPS, continuous, internal 2.048 V reference, data counter on.
    Config c;
    TEST_ASSERT_TRUE(valid(c));
    TEST_ASSERT_TRUE(c.mux == kEmgMux);
    TEST_ASSERT_EQUAL_HEX8(0x01, reg0(c));
    TEST_ASSERT_EQUAL_HEX8(0xD8, reg1(c));
    TEST_ASSERT_EQUAL_HEX8(0x40, reg2(c));
    TEST_ASSERT_EQUAL_HEX8(0x00, reg3(c));
    TEST_ASSERT_EQUAL_FLOAT(2000.0f, samples_per_second(c.rate, c.turbo));
}

static void test_impedance_input_is_ain2_to_avss() {
    Config c;
    c.mux = single_ended(kImpedanceAdcInput);
    TEST_ASSERT_EQUAL_HEX8(0xA1, reg0(c));
    TEST_ASSERT_TRUE(kImpedanceAdcInput != kUnconnectedAdcInput);
}

static void test_single_shot_internal_reference_config() {
    Config c;
    c.mux = Mux::AvddMonitor;
    c.vref = Vref::Internal2048;
    c.rate = DataRate::Dr20;
    c.turbo = false;
    c.continuous = false;
    c.data_counter = false;
    TEST_ASSERT_EQUAL_HEX8(0xD1, reg0(c));
    TEST_ASSERT_EQUAL_HEX8(0x00, reg1(c));
    TEST_ASSERT_EQUAL_HEX8(0x00, reg2(c));
}

static void test_forbidden_configs_are_rejected() {
    Config c;
    c.gain_log2 = 3;  // gain 8 needs the PGA
    TEST_ASSERT_FALSE(valid(c));
    c.pga_bypass = false;
    TEST_ASSERT_TRUE(valid(c));
    Config e;
    e.vref = Vref::External;  // nothing wired to REFP/REFN
    TEST_ASSERT_FALSE(valid(e));
}

// --- ADS122C04 data ---

static void test_24_bit_codes_sign_extend() {
    TEST_ASSERT_EQUAL_INT32(8388607, sign_extend_24(0x7F, 0xFF, 0xFF));
    TEST_ASSERT_EQUAL_INT32(-8388608, sign_extend_24(0x80, 0x00, 0x00));
    TEST_ASSERT_EQUAL_INT32(-1, sign_extend_24(0xFF, 0xFF, 0xFF));
    TEST_ASSERT_EQUAL_INT32(0x123456, sign_extend_24(0x12, 0x34, 0x56));
}

static void test_half_scale_code_is_half_the_reference() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.5f, code_to_volts(4194304, 5.0f));
}

static void test_avdd_monitor_recovers_the_supply() {
    // 5.0 V supply -> monitor reads 1.25 V against 2.048 V.
    const auto code = static_cast<std::int32_t>(1.25 / 2.048 * 8388608.0);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 5.0f, avdd_from_monitor(code));
}

// --- EMG scaling and sample counter ---

static void test_zero_code_is_zero_millivolts() {
    // Differential against ISO_VCM: no midpoint to subtract.
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, electrode_mv(0, kInternalRefV, kFrontEndGain));
}

static void test_one_millivolt_at_electrodes_scales_through_the_front_end() {
    // 1 mV at the electrodes -> 1 mV x 50.2 between AIN0 and AIN1.
    const double v_adc = 0.001 * static_cast<double>(kFrontEndGain);
    const auto code = static_cast<std::int32_t>(v_adc / 2.048 * 8388608.0);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, electrode_mv(code, kInternalRefV, kFrontEndGain));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -1.0f, electrode_mv(-code, kInternalRefV, kFrontEndGain));
}

static void test_full_scale_is_about_21_mv_at_the_electrodes() {
    // +/-2.048 V at the ADC over the measured gain of 97.5.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.0f, electrode_mv(0x7FFFFF, kInternalRefV, kFrontEndGain));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.8f,
                             electrode_mv(0x7FFFFF, kInternalRefV, kNominalFrontEndGain));
}

static void test_full_scale_codes_are_clipped() {
    TEST_ASSERT_TRUE(is_clipped(sign_extend_24(0x7F, 0xFF, 0xFF)));
    TEST_ASSERT_TRUE(is_clipped(sign_extend_24(0x80, 0x00, 0x00)));
    TEST_ASSERT_FALSE(is_clipped(sign_extend_24(0x7F, 0xFF, 0xFE)));
    TEST_ASSERT_FALSE(is_clipped(sign_extend_24(0x80, 0x00, 0x01)));
    TEST_ASSERT_FALSE(is_clipped(sign_extend_24(0xFF, 0xFF, 0xFF)));
    TEST_ASSERT_FALSE(is_clipped(0));
}

static void test_emg_polling_needs_the_stream_speed() {
    // A 400 us poll (2500 Hz) fits at 400 kHz, not at the 100 kHz bring-up speed.
    TEST_ASSERT_TRUE(polling_fits(kI2cStreamHz, 400));
    TEST_ASSERT_FALSE(polling_fits(kI2cBringupHz, 400));
    TEST_ASSERT_FALSE(polling_fits(0, 400));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 165.0f, rdata_transaction_us(400000));
}

static void test_counter_advance_handles_duplicates_misses_and_wrap() {
    TEST_ASSERT_EQUAL_INT(0, counter_advance(5, 5));
    TEST_ASSERT_EQUAL_INT(1, counter_advance(5, 6));
    TEST_ASSERT_EQUAL_INT(1, counter_advance(255, 0));
    TEST_ASSERT_EQUAL_INT(9, counter_advance(250, 3));
}

// --- AD5241 ---

static void test_pot_instruction_bytes() {
    TEST_ASSERT_EQUAL_HEX8(0x00, ad5241::instruction());
    TEST_ASSERT_EQUAL_HEX8(0x20, ad5241::instruction({false, true}));
    TEST_ASSERT_EQUAL_HEX8(0x40, ad5241::instruction({true, false}));
}

static void test_pot_wiper_voltage() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, ad5241::stim_level_volts(0));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.65f, ad5241::stim_level_volts(128));
}

// --- Amplitude table ---

static AmplitudeTable sample_table() {
    return AmplitudeTable({{0.0f, 0}, {10.0f, 80}, {30.0f, 240}});
}

static void test_table_interpolates_inside_measured_range() {
    const AmplitudeTable t = sample_table();
    TEST_ASSERT_TRUE(t.valid());
    std::uint8_t code = 0;
    TEST_ASSERT_TRUE(t.code_for_ma(5.0f, &code));
    TEST_ASSERT_EQUAL_UINT8(40, code);
    TEST_ASSERT_TRUE(t.code_for_ma(20.0f, &code));
    TEST_ASSERT_EQUAL_UINT8(160, code);
    TEST_ASSERT_TRUE(t.code_for_ma(30.0f, &code));
    TEST_ASSERT_EQUAL_UINT8(240, code);
    float ma = 0.0f;
    TEST_ASSERT_TRUE(t.ma_for_code(160, &ma));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 20.0f, ma);
}

static void test_table_never_extrapolates() {
    const AmplitudeTable t = sample_table();
    std::uint8_t code = 0;
    TEST_ASSERT_FALSE(t.code_for_ma(30.1f, &code));
    TEST_ASSERT_FALSE(t.code_for_ma(-0.1f, &code));
    float ma = 0.0f;
    TEST_ASSERT_FALSE(t.ma_for_code(241, &ma));
}

static void test_malformed_tables_are_invalid() {
    TEST_ASSERT_FALSE(AmplitudeTable().valid());
    TEST_ASSERT_FALSE(AmplitudeTable({{0.0f, 0}}).valid());
    TEST_ASSERT_FALSE(AmplitudeTable({{0.0f, 0}, {10.0f, 80}, {5.0f, 120}}).valid());
    TEST_ASSERT_FALSE(AmplitudeTable({{0.0f, 0}, {10.0f, 80}, {20.0f, 80}}).valid());
    std::uint8_t code = 0;
    TEST_ASSERT_FALSE(AmplitudeTable().code_for_ma(1.0f, &code));
}

// --- Board constants ---

static void test_stim_out_divider_matches_schematic() {
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.02166f, kStimOutDivider);
    // +80 V at STIM_OUT -> ~1.73 V at IMP_SENSE, inside the 2.048 V reference.
    TEST_ASSERT_FLOAT_WITHIN(1e-2f, 1.733f, 80.0f * kStimOutDivider);
    TEST_ASSERT_TRUE(80.0f * kStimOutDivider < kInternalRefV);
}

static void test_adc_address_collides_with_the_board_bus_but_pot_does_not() {
    // ADR-0015: why the shield has its own bus.
    TEST_ASSERT_TRUE(on_board_bus(kAdcAddress));
    TEST_ASSERT_EQUAL_HEX8(kEs7210Address, kAdcAddress);
    TEST_ASSERT_FALSE(on_board_bus(kPotAddress));
    TEST_ASSERT_TRUE(kShieldI2cPort != kBoardI2cPort);
    TEST_ASSERT_TRUE(kShieldI2cSdaGpio != kBoardI2cSdaGpio);
    TEST_ASSERT_TRUE(kShieldI2cSclGpio != kBoardI2cSclGpio);
}

static void test_nominal_current_and_bench_cap() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, nominal_ma_for_code(0));
    // Full-scale ideal command is ~60 mA: twice the board's 30 mA target.
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, nominal_ma_for_code(255));
    // The bench cap stays well under the 30 mA maximum.
    TEST_ASSERT_TRUE(nominal_ma_for_code(kBenchMaxPulseCode) < 5.0f);
    TEST_ASSERT_TRUE(nominal_ma_for_code(kBenchMaxPulseCode) < hal::kCurrentMaxMa);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_register_commands_carry_the_register_number);
    RUN_TEST(test_emg_sampling_config_encodes_as_expected);
    RUN_TEST(test_impedance_input_is_ain2_to_avss);
    RUN_TEST(test_single_shot_internal_reference_config);
    RUN_TEST(test_forbidden_configs_are_rejected);
    RUN_TEST(test_24_bit_codes_sign_extend);
    RUN_TEST(test_half_scale_code_is_half_the_reference);
    RUN_TEST(test_avdd_monitor_recovers_the_supply);
    RUN_TEST(test_zero_code_is_zero_millivolts);
    RUN_TEST(test_one_millivolt_at_electrodes_scales_through_the_front_end);
    RUN_TEST(test_full_scale_is_about_21_mv_at_the_electrodes);
    RUN_TEST(test_full_scale_codes_are_clipped);
    RUN_TEST(test_emg_polling_needs_the_stream_speed);
    RUN_TEST(test_counter_advance_handles_duplicates_misses_and_wrap);
    RUN_TEST(test_pot_instruction_bytes);
    RUN_TEST(test_pot_wiper_voltage);
    RUN_TEST(test_table_interpolates_inside_measured_range);
    RUN_TEST(test_table_never_extrapolates);
    RUN_TEST(test_malformed_tables_are_invalid);
    RUN_TEST(test_stim_out_divider_matches_schematic);
    RUN_TEST(test_adc_address_collides_with_the_board_bus_but_pot_does_not);
    RUN_TEST(test_nominal_current_and_bench_cap);
    return UNITY_END();
}
