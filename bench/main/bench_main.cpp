// Bench firmware: a serial command line for bringing up the shield drivers.
//
// BENCH USE ONLY. It stimulates. Connect resistive dummy loads to CN1, never
// a person.
//
// Suggested order, each step relying on the one before:
//   scan          the four I2C addresses answer (ADR-0010, -0013)
//   adc_inputs    AVDD ~5 V; ADC_SIGNAL's input sits near 2.5 V; AIN3 ~0 V
//   pot 128       readback matches; STIM_LEVEL stays 0 V (output disabled)
//   emg_start     emg_stats shows ~2000 SPS, few missed, no bus errors
//   pulse 64 250  scope TP11/TP10: one 250 us pulse, current = dV(R29)/20
//   train 64 250  four pulses 500 ms apart
//   imp 64 250    averaged STIM_OUT reading, for the ADR-0014 table

#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "driver/i2c_master.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "shield/ad5241.hpp"
#include "shield/ad5241_regs.hpp"
#include "shield/ads122c04.hpp"
#include "shield/board.hpp"
#include "shield/pulse_generator.hpp"
#include "shield/shield_emg.hpp"
#include "shield/shield_stimulator.hpp"

using namespace shield;
using namespace shield::ads122c04;

namespace {

i2c_master_bus_handle_t g_bus = nullptr;
Ads122c04* g_adc = nullptr;
Ad5241* g_pot = nullptr;
PulseGenerator* g_pulse = nullptr;
ShieldStimulator* g_stim = nullptr;
ShieldEmg* g_emg = nullptr;

// ---- Helpers ----------------------------------------------------------------

bool parse_long(const char* s, long lo, long hi, long* out) {
    if (s == nullptr) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const long v = std::strtol(s, &end, 0);
    if (errno != 0 || end == s || *end != '\0' || v < lo || v > hi) {
        return false;
    }
    *out = v;
    return true;
}

bool parse_code_and_width(int argc, char** argv, std::uint8_t* code, std::uint32_t* width) {
    long c = 0;
    long w = 0;
    if (argc != 3 || !parse_long(argv[1], 0, 255, &c) ||
        !parse_long(argv[2], hal::kPulseWidthMinUs, hal::kPulseWidthMaxUs, &w)) {
        std::printf("usage: %s <pot code 0-255> <width us %" PRIu32 "-%" PRIu32 ">\n", argv[0],
                    hal::kPulseWidthMinUs, hal::kPulseWidthMaxUs);
        return false;
    }
    *code = static_cast<std::uint8_t>(c);
    *width = static_cast<std::uint32_t>(w);
    return true;
}

bool stimulator_ready() {
    if (g_stim->faulted()) {
        std::printf("stimulator is FAULTED (see `status`); `clear_fault` after fixing the cause\n");
        return false;
    }
    return true;
}

// Stops EMG sampling for commands that reconfigure the ADC; returns whether it
// was running so the caller can restart it.
bool pause_emg() {
    const bool was = g_emg->running();
    if (was) {
        g_emg->stop();
    }
    return was;
}

void resume_emg(bool was) {
    if (was && !g_emg->start()) {
        std::printf("EMG restart failed\n");
    }
}

// ---- Commands ---------------------------------------------------------------

int cmd_scan(int, char**) {
    std::printf("I2C devices on port %d (SDA %d, SCL %d):\n", kI2cPort, kI2cSdaGpio, kI2cSclGpio);
    for (std::uint16_t a = 0x08; a < 0x78; ++a) {
        if (i2c_master_probe(g_bus, a, 10) == ESP_OK) {
            const char* what = "";
            if (a == kAdcAddress) what = "  ADS122C04 (shield ADC)";
            if (a == kPotAddress) what = "  AD5241 (shield pot)";
            if (a == 0x5D || a == 0x14) what = "  GT911 touch";
            if (a == 0x18) what = "  ES8311 codec";
            std::printf("  0x%02X%s\n", a, what);
        }
    }
    return 0;
}

int cmd_adc_inputs(int, char**) {
    const bool was = pause_emg();
    std::int32_t code = 0;
    esp_err_t err = g_adc->read_single(Mux::AvddMonitor, Vref::Internal2048, &code);
    if (err != ESP_OK) {
        std::printf("AVDD monitor read failed: %s\n", esp_err_to_name(err));
        resume_emg(was);
        return 1;
    }
    const float avdd = avdd_from_monitor(code);
    std::printf("AVDD (ISO_5V)     %.4f V\n", static_cast<double>(avdd));
    for (int ain = 0; ain < 4; ++ain) {
        err = g_adc->read_single(single_ended(ain), Vref::AnalogSupply, &code);
        if (err != ESP_OK) {
            std::printf("AIN%d read failed: %s\n", ain, esp_err_to_name(err));
            continue;
        }
        const char* note = "";
        if (ain == kEmgAdcInput) note = "  <- configured EMG input (expect ~AVDD/2)";
        if (ain == kImpedanceAdcInput) note = "  <- IMP_SENSE (expect ~0 V)";
        std::printf("AIN%d              %.4f V  (code %" PRId32 ")%s\n", ain,
                    static_cast<double>(code_to_volts(code, avdd)), code, note);
    }
    resume_emg(was);
    return 0;
}

int cmd_emg_start(int argc, char** argv) {
    if (argc == 2) {
        long ain = 0;
        if (!parse_long(argv[1], 0, 3, &ain)) {
            std::printf("usage: emg_start [ain 0-3]\n");
            return 1;
        }
        g_emg->stop();
        g_emg->set_input(static_cast<int>(ain));
    }
    if (!g_emg->start()) {
        std::printf("EMG start failed (ADC not answering, or AVDD out of range)\n");
        return 1;
    }
    std::printf("EMG sampling; `emg_stats` in a second or two\n");
    return 0;
}

int cmd_emg_stop(int, char**) {
    g_emg->stop();
    return 0;
}

int cmd_emg_stats(int, char**) {
    const EmgStats s = g_emg->stats();
    std::printf("running %d  error %d\n", g_emg->running() ? 1 : 0, g_emg->error() ? 1 : 0);
    std::printf("AVDD %.4f V   rate %.1f SPS (target 2000)\n", static_cast<double>(s.vref_v),
                static_cast<double>(s.measured_sps));
    std::printf("polls %" PRIu32 "  fresh %" PRIu32 "  duplicate %" PRIu32 "  missed %" PRIu32
                "  bus errors %" PRIu32 "\n",
                s.polls, s.fresh, s.duplicates, s.missed, s.bus_errors);

    std::vector<float> w(2000);
    const std::size_t n = g_emg->copy_recent(w.data(), w.size());
    double sum = 0.0;
    double sq = 0.0;
    std::size_t valid = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isnan(w[i])) {
            sum += w[i];
            sq += static_cast<double>(w[i]) * w[i];
            ++valid;
        }
    }
    if (valid > 0) {
        const double mean = sum / static_cast<double>(valid);
        const double rms = std::sqrt(sq / static_cast<double>(valid) - mean * mean);
        std::printf("last %u samples: mean %.4f mV  AC rms %.4f mV (at electrodes)\n",
                    static_cast<unsigned>(valid), mean, rms);
    }
    return 0;
}

int cmd_emg_dump(int argc, char** argv) {
    long n = 2000;
    if (argc == 2 && !parse_long(argv[1], 1, 8192, &n)) {
        std::printf("usage: emg_dump [samples 1-8192]\n");
        return 1;
    }
    std::vector<float> w(static_cast<std::size_t>(n));
    const std::uint32_t end = g_emg->sample_count();
    const std::size_t got = g_emg->copy_recent(w.data(), w.size());
    const std::uint32_t first = end - static_cast<std::uint32_t>(got);
    // CSV for MATLAB: sample index, millivolts at the electrodes. NaN = missed.
    std::printf("index,mv\n");
    for (std::size_t i = 0; i < got; ++i) {
        std::printf("%" PRIu32 ",%.5f\n", first + static_cast<std::uint32_t>(i),
                    static_cast<double>(w[i]));
        if ((i & 0xFF) == 0) {
            vTaskDelay(1);  // let the UART drain and the idle task run
        }
    }
    return 0;
}

int cmd_pot(int argc, char** argv) {
    if (argc == 2) {
        long c = 0;
        if (!parse_long(argv[1], 0, 255, &c)) {
            std::printf("usage: pot [code 0-255]\n");
            return 1;
        }
        if (!stimulator_ready()) {
            return 1;
        }
        if (!g_stim->set_amplitude_code(static_cast<std::uint8_t>(c))) {
            std::printf("write/readback failed -> stimulator faulted\n");
            return 1;
        }
    }
    std::uint8_t code = 0;
    const esp_err_t err = g_pot->read(&code);
    if (err != ESP_OK) {
        std::printf("pot read failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    std::printf("pot %u -> STIM_LEVEL %.3f V when output enabled (now %s)\n", code,
                static_cast<double>(ad5241::stim_level_volts(code)),
                g_stim->output_enabled() ? "ENABLED" : "disabled, so 0 V");
    return 0;
}

int cmd_pulse(int argc, char** argv) {
    std::uint8_t code = 0;
    std::uint32_t width = 0;
    if (!parse_code_and_width(argc, argv, &code, &width) || !stimulator_ready()) {
        return 1;
    }
    bool ok = g_stim->set_amplitude_code(code) && g_stim->enable_output(true);
    const std::uint32_t at = g_emg->sample_count();
    ok = ok && g_stim->fire_pulse(width);
    g_stim->enable_output(false);
    std::printf("%s: pot %u, %" PRIu32 " us, at EMG sample %" PRIu32 "\n", ok ? "fired" : "FAILED",
                code, width, at);
    return ok ? 0 : 1;
}

int cmd_train(int argc, char** argv) {
    std::uint8_t code = 0;
    std::uint32_t width = 0;
    if (!parse_code_and_width(argc, argv, &code, &width) || !stimulator_ready()) {
        return 1;
    }
    std::uint32_t at[4] = {};
    int fired = 0;
    if (g_stim->set_amplitude_code(code) && g_stim->enable_output(true)) {
        TickType_t wake = xTaskGetTickCount();
        for (int k = 0; k < 4; ++k) {
            if (k > 0) {
                vTaskDelayUntil(&wake, pdMS_TO_TICKS(500));
            }
            at[k] = g_emg->sample_count();
            if (!g_stim->fire_pulse(width)) {
                break;
            }
            ++fired;
        }
    }
    g_stim->enable_output(false);
    std::printf("%d of 4 pulses fired (pot %u, %" PRIu32 " us)\n", fired, code, width);
    for (int k = 0; k < fired; ++k) {
        std::printf("  T%d at EMG sample %" PRIu32 "\n", k + 1, at[k]);
    }
    if (g_emg->running()) {
        std::printf("`emg_dump 4000` to capture the train\n");
    }
    return fired == 4 ? 0 : 1;
}

// One single-shot conversion of AIN3 (IMP_SENSE), optionally with a pulse
// fired inside the conversion window. ADR-0014: the pulse is shorter than a
// conversion, so the result is an average; it is turned into ohms through a
// table measured against known resistors.
esp_err_t imp_conversion(bool with_pulse, std::uint32_t width, std::int32_t* code) {
    Config c;
    c.mux = single_ended(kImpedanceAdcInput);
    c.vref = Vref::AnalogSupply;
    c.rate = DataRate::Dr1000;
    c.turbo = true;
    c.continuous = false;
    c.data_counter = false;
    esp_err_t err = g_adc->configure(c);
    if (err != ESP_OK) {
        return err;
    }
    if (with_pulse && !g_stim->enable_output(true)) {
        return ESP_FAIL;
    }
    err = g_adc->start();
    if (err == ESP_OK && with_pulse && !g_stim->fire_pulse(width)) {
        err = ESP_FAIL;
    }
    if (with_pulse) {
        g_stim->enable_output(false);
    }
    if (err != ESP_OK) {
        return err;
    }
    for (int i = 0; i < 20; ++i) {
        vTaskDelay(pdMS_TO_TICKS(1));
        std::uint8_t r2 = 0;
        err = g_adc->read_register(2, &r2);
        if (err != ESP_OK) {
            return err;
        }
        if (r2 & kReg2DrdyBit) {
            std::uint8_t unused = 0;
            return g_adc->read_data(code, &unused);
        }
    }
    return ESP_ERR_TIMEOUT;
}

int cmd_imp(int argc, char** argv) {
    std::uint8_t code = 0;
    std::uint32_t width = 0;
    if (!parse_code_and_width(argc, argv, &code, &width) || !stimulator_ready()) {
        return 1;
    }
    const bool was = pause_emg();
    std::int32_t monitor = 0;
    std::int32_t base = 0;
    std::int32_t pulsed = 0;
    esp_err_t err = g_adc->read_single(Mux::AvddMonitor, Vref::Internal2048, &monitor);
    if (err == ESP_OK) {
        err = imp_conversion(false, width, &base);
    }
    if (err == ESP_OK && g_stim->set_amplitude_code(code)) {
        err = imp_conversion(true, width, &pulsed);
    } else if (err == ESP_OK) {
        err = ESP_FAIL;
    }
    g_stim->enable_output(false);
    resume_emg(was);
    if (err != ESP_OK) {
        std::printf("impedance read failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    const float avdd = avdd_from_monitor(monitor);
    const float v_base = code_to_volts(base, avdd);
    const float v_pulse = code_to_volts(pulsed, avdd);
    std::printf("pot %u, %" PRIu32 " us\n", code, width);
    std::printf("  no pulse   code %8" PRId32 "  %.5f V at AIN3\n", base, static_cast<double>(v_base));
    std::printf("  with pulse code %8" PRId32 "  %.5f V at AIN3\n", pulsed,
                static_cast<double>(v_pulse));
    std::printf("  difference %.5f V at AIN3 = %.3f V averaged at STIM_OUT\n",
                static_cast<double>(v_pulse - v_base),
                static_cast<double>((v_pulse - v_base) / kStimOutDivider));
    std::printf("  (an average over the conversion, not the pulse voltage: log it against the\n"
                "   load resistor to build the ADR-0014 table)\n");
    return 0;
}

int cmd_status(int, char**) {
    std::printf("stimulator: %s, output %s, pot code %u\n",
                g_stim->faulted() ? "FAULTED" : "ok",
                g_stim->output_enabled() ? "ENABLED" : "disabled", g_stim->amplitude_code());
    std::printf("EMG: %s%s\n", g_emg->running() ? "running" : "stopped",
                g_emg->error() ? ", ERROR (bus)" : "");
    std::printf("pins: PULSE GPIO%d, OUTPUT_ENABLE GPIO%d (to verify against J1)\n",
                kStimPulseGpio, kStimOutputEnableGpio);
    return 0;
}

int cmd_clear_fault(int, char**) {
    g_stim->clear_fault();
    std::printf("stimulator fault cleared\n");
    return 0;
}

int cmd_bus_reset(int, char**) {
    const esp_err_t err = i2c_master_bus_reset(g_bus);
    std::printf("bus reset: %s\n", esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

void register_command(const char* name, const char* help, const char* hint,
                      esp_console_cmd_func_t fn) {
    esp_console_cmd_t c{};
    c.command = name;
    c.help = help;
    c.hint = hint;
    c.func = fn;
    ESP_ERROR_CHECK(esp_console_cmd_register(&c));
}

void register_commands() {
    register_command("scan", "List devices answering on the I2C bus", nullptr, cmd_scan);
    register_command("adc_inputs", "Read AVDD and AIN0-3 as DC voltages", nullptr, cmd_adc_inputs);
    register_command("emg_start", "Start 2000 SPS EMG sampling", "[ain]", cmd_emg_start);
    register_command("emg_stop", "Stop EMG sampling", nullptr, cmd_emg_stop);
    register_command("emg_stats", "Sample rate, missed/duplicate counts, noise", nullptr,
                     cmd_emg_stats);
    register_command("emg_dump", "Print recent samples as CSV (index,mv)", "[samples]",
                     cmd_emg_dump);
    register_command("pot", "Read, or write and verify, the amplitude pot", "[code]", cmd_pot);
    register_command("pulse", "One pulse: set pot, enable, fire, disable", "<code> <width_us>",
                     cmd_pulse);
    register_command("train", "Four pulses 500 ms apart, output enabled only for the train",
                     "<code> <width_us>", cmd_train);
    register_command("imp", "AIN3 single-shot with and without a pulse (ADR-0014)",
                     "<code> <width_us>", cmd_imp);
    register_command("status", "Stimulator and EMG state", nullptr, cmd_status);
    register_command("clear_fault", "Clear a latched stimulator fault", nullptr, cmd_clear_fault);
    register_command("bus_reset", "Clock out a stuck I2C bus", nullptr, cmd_bus_reset);
}

}  // namespace

extern "C" void app_main() {
    // Claim the stimulator lines low before anything else can fail.
    g_pulse = new PulseGenerator(kStimPulseGpio);
    const esp_err_t pulse_err = g_pulse->init();

    // Same port, pins and settings as the BSP's bus (ADR-0013). The bench does
    // not link the BSP, so it creates the bus itself; the device build must
    // use bsp_i2c_get_handle() instead.
    i2c_master_bus_config_t bc{};
    bc.i2c_port = kI2cPort;
    bc.sda_io_num = static_cast<gpio_num_t>(kI2cSdaGpio);
    bc.scl_io_num = static_cast<gpio_num_t>(kI2cSclGpio);
    bc.clk_source = I2C_CLK_SRC_DEFAULT;
    bc.glitch_ignore_cnt = 7;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bc, &g_bus));

    g_adc = new Ads122c04(g_bus, kAdcAddress, kI2cSpeedHz);
    g_pot = new Ad5241(g_bus, kPotAddress, kI2cSpeedHz);
    const esp_err_t adc_err = g_adc->init();
    const esp_err_t pot_err = g_pot->init();
    g_stim = new ShieldStimulator(*g_pot, *g_pulse, kStimOutputEnableGpio, AmplitudeTable{});
    const esp_err_t stim_err = g_stim->init();
    g_emg = new ShieldEmg(*g_adc, g_bus, kEmgAdcInput, kFrontEndGain);

    std::printf("\n"
                "==============================================================\n"
                " NMT shield bench firmware\n"
                " BENCH USE ONLY: this stimulates. Resistive dummy loads only,\n"
                " never a person.\n"
                "==============================================================\n");
    std::printf("pulse generator: %s\n", esp_err_to_name(pulse_err));
    std::printf("ADS122C04 reset: %s\n", esp_err_to_name(adc_err));
    std::printf("AD5241 add:      %s\n", esp_err_to_name(pot_err));
    std::printf("stimulator init: %s%s\n", esp_err_to_name(stim_err),
                stim_err != ESP_OK ? " (stimulator faulted; check `scan`)" : "");
    std::printf("Type `help`. Start with `scan`.\n\n");

    esp_console_repl_t* repl = nullptr;
    esp_console_repl_config_t rc = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    rc.prompt = "bench>";
    esp_console_dev_uart_config_t uc = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uc, &rc, &repl));
    esp_console_register_help_command();
    register_commands();
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
