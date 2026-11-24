// Bench firmware: a serial command line for bringing up the shield drivers.
//
// Two builds (see ../CMakeLists.txt):
//   default (NMT_BENCH_STIM=0)  NO STIMULATION. STIM_PULSE and
//       STIM_OUTPUT_ENABLE are plain GPIO outputs held low, and no command
//       can change them. The pulse generator is not linked in.
//   NMT_BENCH_STIM=1            adds pulse/train/imp. Resistive dummy loads on
//       CN1 only, never a person. Pot codes for pulses are capped at
//       kBenchMaxPulseCode until a measured amplitude table exists.
//
// The shield's I2C is on its own bus (ADR-0015): its ADS122C04 is at 0x40,
// the same address as the Waveshare board's ES7210 microphone ADC.
//
// Suggested order, each step relying on the one before:
//   (boot)        both buses scanned automatically, address only
//   scan          board bus: GT911, 0x18, 0x40 (ES7210). Shield bus: 0x2C, 0x40
//   adc_reset     RESET, then all four registers read back 0 -> ADC ready
//   adc_inputs    AVDD ~5 V; AIN0, AIN1 ~2.5 V; AIN0-AIN1 ~0 V
//   stim_out_dc   STIM_OUT idle voltage through the divider (DC only)
//   pot 128       readback matches; STIM_LEVEL stays 0 V (output disabled)
//   emg_start     emg_stats shows ~2000 SPS, few missed, no bus errors
// Stimulating build only:
//   pulse 10 250  scope TP11/TP10: one 250 us pulse, current = dV(R29)/20
//   train 10 250  four pulses 500 ms apart
//   imp 10 250    averaged STIM_OUT reading, for the ADR-0014 table

#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_console.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/stimulator.hpp"
#include "shield/ad5241.hpp"
#include "shield/ad5241_regs.hpp"
#include "shield/ads122c04.hpp"
#include "shield/board.hpp"
#include "shield/sample_math.hpp"
#include "shield/shield_emg.hpp"

#if NMT_BENCH_STIM
#include "shield/pulse_generator.hpp"
#include "shield/shield_stimulator.hpp"
#endif

using namespace shield;
using namespace shield::ads122c04;

namespace {

i2c_master_bus_handle_t g_board_bus = nullptr;
i2c_master_bus_handle_t g_shield_bus = nullptr;
Ads122c04* g_adc = nullptr;
Ad5241* g_pot = nullptr;
ShieldEmg* g_emg = nullptr;
// Set only by a successful adc_reset: the chip at 0x40 on the shield bus
// behaved like an ADS122C04. Nothing else talks to the ADC until then.
bool g_adc_ready = false;

#if NMT_BENCH_STIM
PulseGenerator* g_pulse = nullptr;
ShieldStimulator* g_stim = nullptr;
#endif

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

const char* reset_reason_name(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON: return "power-on";
        case ESP_RST_EXT: return "external pin";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INTERRUPT WATCHDOG";
        case ESP_RST_TASK_WDT: return "TASK WATCHDOG";
        case ESP_RST_WDT: return "WATCHDOG";
        case ESP_RST_DEEPSLEEP: return "deep sleep";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_USB: return "USB";
        default: return "other";
    }
}

// Drives a pin low as an output: level first, then direction, so it never
// goes high even briefly.
void hold_low(int gpio) {
    const auto pin = static_cast<gpio_num_t>(gpio);
    gpio_set_level(pin, 0);
    gpio_config_t gc{};
    gc.pin_bit_mask = 1ULL << gpio;
    gc.mode = GPIO_MODE_OUTPUT;
    gc.pull_down_en = GPIO_PULLDOWN_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&gc));
    gpio_set_level(pin, 0);
}

i2c_master_bus_handle_t new_bus(int port, int sda, int scl, bool internal_pullups) {
    i2c_master_bus_config_t bc{};
    bc.i2c_port = port;
    bc.sda_io_num = static_cast<gpio_num_t>(sda);
    bc.scl_io_num = static_cast<gpio_num_t>(scl);
    bc.clk_source = I2C_CLK_SRC_DEFAULT;
    bc.glitch_ignore_cnt = 7;
    bc.flags.enable_internal_pullup = internal_pullups;
    i2c_master_bus_handle_t bus = nullptr;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bc, &bus));
    return bus;
}

const char* board_bus_name(std::uint8_t a) {
    if (a == kGt911AddressA || a == kGt911AddressB) return "GT911 touch";
    if (a == kEs8311Address) return "ES8311 speaker codec";
    if (a == kEs7210Address) return "ES7210 microphone ADC";
    if (a == kPotAddress) return "?? AD5241 address: is the shield still on pins 4/6?";
    return "?? unexpected";
}

const char* shield_bus_name(std::uint8_t a) {
    if (a == kAdcAddress) return "ADS122C04 address (identity checked by adc_reset)";
    if (a == kPotAddress) return "AD5241 address";
    if (on_board_bus(a)) return "?? a board-bus address: is this wired to pins 4/6?";
    return "?? unexpected";
}

// Address-only scan: a START, the address, and a STOP. No data is written.
int scan_bus(const char* label, i2c_master_bus_handle_t bus, int port, int sda, int scl,
             const char* (*name)(std::uint8_t)) {
    std::printf("%s bus (port %d, SDA GPIO%d, SCL GPIO%d), address-only scan:\n", label, port, sda,
                scl);
    int found = 0;
    for (std::uint16_t a = 0x08; a < 0x78; ++a) {
        const esp_err_t err = i2c_master_probe(bus, a, 10);
        if (err == ESP_OK) {
            std::printf("  0x%02X  %s\n", a, name(static_cast<std::uint8_t>(a)));
            ++found;
        } else if (err != ESP_ERR_NOT_FOUND) {
            std::printf("  bus error at 0x%02X: %s (lines held low? pull-ups? power?)\n", a,
                        esp_err_to_name(err));
            return -1;
        }
    }
    if (found == 0) {
        std::printf("  nothing answered\n");
    }
    return found;
}

// Result of the boot-time wiring check (check_shield_lines).
bool g_sda_pulled_up = false;
bool g_scl_pulled_up = false;

// Reads a pin with the P4's internal pull-down (~45 kOhm) on and nothing else
// driving it. The shield's R2/R4 (3.3 kOhm to HOST_3V3) overpower it, so a pin
// wired to the shield reads 1, and an unconnected pin reads 0. HOST_3V3 comes
// from the Waveshare (J1 pin 2), so this works with the shield's own 5 V off.
// Must run before the I2C driver takes the pins.
bool pulled_up_externally(int gpio) {
    gpio_config_t gc{};
    gc.pin_bit_mask = 1ULL << gpio;
    gc.mode = GPIO_MODE_INPUT;
    gc.pull_up_en = GPIO_PULLUP_DISABLE;
    gc.pull_down_en = GPIO_PULLDOWN_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&gc));
    vTaskDelay(pdMS_TO_TICKS(2));
    return gpio_get_level(static_cast<gpio_num_t>(gpio)) == 1;
}

void check_shield_lines() {
    g_sda_pulled_up = pulled_up_externally(kShieldI2cSdaGpio);
    g_scl_pulled_up = pulled_up_externally(kShieldI2cSclGpio);
}

void print_shield_lines() {
    std::printf("Shield I2C wiring (internal pull-down vs the shield's 3.3 kOhm pull-ups):\n");
    std::printf("  SDA GPIO%d (header 16): %s\n", kShieldI2cSdaGpio,
                g_sda_pulled_up ? "pulled up -> reaches the shield" : "NOT pulled up");
    std::printf("  SCL GPIO%d (header 20): %s\n", kShieldI2cSclGpio,
                g_scl_pulled_up ? "pulled up -> reaches the shield" : "NOT pulled up");
    if (!g_sda_pulled_up || !g_scl_pulled_up) {
        std::printf("  -> a line not pulled up is not connected to shield J1 pin 4/6, or J1 pin 2\n"
                    "     (HOST_3V3) is not jumpered to header pin 2\n");
    }
}

void scan_shield_bus() {
    const int n = scan_bus("Shield", g_shield_bus, kShieldI2cPort, kShieldI2cSdaGpio,
                           kShieldI2cSclGpio, shield_bus_name);
    if (n == 0) {
        if (g_sda_pulled_up && g_scl_pulled_up) {
            std::printf("  -> wiring reaches the shield (at boot), but nothing behind the\n"
                        "     isolator answers: is the shield's 5 V supply on? (TP6 ~3.2 V)\n"
                        "     Or SDA/SCL swapped: J1 4 -> header 16, J1 6 -> header 20.\n");
        } else {
            std::printf("  -> fix the wiring above first\n");
        }
    }
}

void scan_board_bus() {
    scan_bus("Board", g_board_bus, kBoardI2cPort, kBoardI2cSdaGpio, kBoardI2cSclGpio,
             board_bus_name);
}

bool adc_ready() {
    if (!g_adc_ready) {
        std::printf("ADC not ready: run `adc_reset` first\n");
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

const char* emg_start_error_text(EmgStartError e) {
    switch (e) {
        case EmgStartError::None: return "none";
        case EmgStartError::BusTooSlow: return "ADC SCL too slow to poll at 2000 SPS";
        case EmgStartError::AdcNoAnswer: return "ADC did not answer (AVDD monitor read)";
        case EmgStartError::AvddOutOfRange: return "AVDD (ISO_5V) out of 4.5-5.5 V";
        case EmgStartError::ConfigFailed: return "configure/readback or START failed";
        case EmgStartError::NoResources: return "could not create task or timer";
    }
    return "?";
}

bool output_enabled() {
#if NMT_BENCH_STIM
    return g_stim->output_enabled();
#else
    return false;  // held low by hold_low(); nothing in this build raises it
#endif
}

// ---- Commands: buses and ADC ------------------------------------------------

int cmd_scan(int argc, char** argv) {
    const bool board = argc < 2 || std::strcmp(argv[1], "board") == 0;
    const bool shield = argc < 2 || std::strcmp(argv[1], "shield") == 0;
    if (!board && !shield) {
        std::printf("usage: scan [board|shield]\n");
        return 1;
    }
    if (board) scan_board_bus();
    if (shield) {
        std::printf("(wiring check below is from boot; press RST to redo it)\n");
        print_shield_lines();
        scan_shield_bus();
    }
    return 0;
}

int cmd_adc_reset(int, char**) {
    if (g_emg->running()) {
        std::printf("stop EMG first (`emg_stop`)\n");
        return 1;
    }
    g_adc_ready = false;
    const esp_err_t err = g_adc->reset();
    if (err == ESP_ERR_INVALID_RESPONSE) {
        std::printf("RESET sent, but registers did not read back as 0: not an ADS122C04, or a\n"
                    "second device at 0x40 on this bus. `adc_regs` shows what came back.\n");
        return 1;
    }
    if (err != ESP_OK) {
        std::printf("ADC reset failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    g_adc_ready = true;
    std::printf("ADC reset; registers read back at power-on values. ADC ready.\n");
    return 0;
}

int cmd_adc_regs(int, char**) {
    for (std::uint8_t r = 0; r < 4; ++r) {
        std::uint8_t v = 0;
        const esp_err_t err = g_adc->read_register(r, &v);
        if (err != ESP_OK) {
            std::printf("reg %u: %s\n", r, esp_err_to_name(err));
            return 1;
        }
        std::printf("reg %u: 0x%02X\n", r, v);
    }
    return 0;
}

int cmd_adc_inputs(int, char**) {
    if (!adc_ready()) {
        return 1;
    }
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
    static const char* const kNotes[4] = {
        "ADC_SIGNAL (front-end output, expect ~2.5 V)",
        "ISO_VCM (expect ~2.5 V)",
        "IMP_SENSE (STIM_OUT / 46.2; ~1.65 V if STIM_OUT idles near +76 V open-circuit)",
        "unconnected (floats; any value)",
    };
    for (int ain = 0; ain < 4; ++ain) {
        err = g_adc->read_single(single_ended(ain), Vref::AnalogSupply, &code);
        if (err != ESP_OK) {
            std::printf("AIN%d read failed: %s\n", ain, esp_err_to_name(err));
            continue;
        }
        std::printf("AIN%d              %.4f V  (code %8" PRId32 ")  %s\n", ain,
                    static_cast<double>(code_to_volts(code, avdd)), code, kNotes[ain]);
    }
    err = g_adc->read_single(kEmgMux, Vref::Internal2048, &code);
    if (err == ESP_OK) {
        std::printf("AIN0-AIN1         %+.5f V  (code %8" PRId32 ")  EMG input, expect near 0\n",
                    static_cast<double>(code_to_volts(code, kInternalRefV)), code);
    }
    err = g_adc->read_single(Mux::Shorted, Vref::Internal2048, &code);
    if (err == ESP_OK) {
        std::printf("shorted           %+.6f V  (code %8" PRId32 ")  ADC offset\n",
                    static_cast<double>(code_to_volts(code, kInternalRefV)), code);
    }
    resume_emg(was);
    return 0;
}

// IMP_SENSE against the internal reference: STIM_OUT's DC level, no pulse.
int cmd_stim_out_dc(int, char**) {
    if (!adc_ready()) {
        return 1;
    }
    const bool was = pause_emg();
    std::int32_t code = 0;
    const esp_err_t err =
        g_adc->read_single(single_ended(kImpedanceAdcInput), Vref::Internal2048, &code);
    resume_emg(was);
    if (err != ESP_OK) {
        std::printf("AIN%d read failed: %s\n", kImpedanceAdcInput, esp_err_to_name(err));
        return 1;
    }
    const float v = code_to_volts(code, kInternalRefV);
    std::printf("IMP_SENSE %.4f V -> STIM_OUT %.2f V (DC, through the 46.2:1 divider)%s\n",
                static_cast<double>(v), static_cast<double>(v / kStimOutDivider),
                is_clipped(code) ? "  CLIPPED (STIM_OUT above ~94 V?)" : "");
    std::printf("Compare with a meter at TP12 (IMP_SENSE) and TP10 (STIM_OUT), to ISO_GND.\n");
    return 0;
}

// ---- Commands: EMG ----------------------------------------------------------

int cmd_emg_start(int, char**) {
    if (!adc_ready()) {
        return 1;
    }
    if (g_adc->scl_hz() != kI2cStreamHz) {
        const esp_err_t err = g_adc->set_scl_hz(kI2cStreamHz);
        if (err != ESP_OK) {
            std::printf("could not switch ADC to %" PRIu32 " Hz: %s\n", kI2cStreamHz,
                        esp_err_to_name(err));
            return 1;
        }
        std::printf("ADC SCL now %" PRIu32 " Hz (100 kHz cannot keep up with 2000 SPS)\n",
                    kI2cStreamHz);
    }
    if (!g_emg->start()) {
        std::printf("EMG start failed: %s\n", emg_start_error_text(g_emg->last_start_error()));
        return 1;
    }
    std::printf("EMG sampling AIN0-AIN1; `emg_stats` in a second or two\n");
    return 0;
}

int cmd_emg_stop(int, char**) {
    g_emg->stop();
    return 0;
}

int cmd_emg_stats(int, char**) {
    const EmgStats s = g_emg->stats();
    std::printf("running %d  error %d\n", g_emg->running() ? 1 : 0, g_emg->error() ? 1 : 0);
    std::printf("AVDD %.4f V  Vref %.3f V  rate %.1f SPS (target 2000)\n",
                static_cast<double>(s.avdd_v), static_cast<double>(s.vref_v),
                static_cast<double>(s.measured_sps));
    std::printf("polls %" PRIu32 "  fresh %" PRIu32 "  duplicate %" PRIu32 "  missed %" PRIu32
                "  clipped %" PRIu32 "  bus errors %" PRIu32 "\n",
                s.polls, s.fresh, s.duplicates, s.missed, s.clipped, s.bus_errors);

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
        std::printf("last %u samples: mean %.4f mV  AC rms %.4f mV (at electrodes, nominal "
                    "gain %.1f)\n",
                    static_cast<unsigned>(valid), mean, rms, static_cast<double>(kFrontEndGain));
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

// ---- Commands: amplitude pot ------------------------------------------------

int cmd_pot(int argc, char** argv) {
    if (argc == 2) {
        long c = 0;
        if (!parse_long(argv[1], 0, 255, &c)) {
            std::printf("usage: pot [code 0-255]\n");
            return 1;
        }
        const auto code = static_cast<std::uint8_t>(c);
#if NMT_BENCH_STIM
        if (g_stim->faulted()) {
            std::printf("stimulator is FAULTED (see `status`); `clear_fault` after fixing it\n");
            return 1;
        }
        if (!g_stim->set_amplitude_code(code)) {
            std::printf("write/readback failed -> stimulator faulted\n");
            return 1;
        }
#else
        const esp_err_t err = g_pot->write_verified(code);
        if (err != ESP_OK) {
            std::printf("pot write/readback failed: %s\n", esp_err_to_name(err));
            return 1;
        }
#endif
    }
    std::uint8_t code = 0;
    const esp_err_t err = g_pot->read(&code);
    if (err != ESP_OK) {
        std::printf("pot read failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    std::printf("pot %u -> STIM_LEVEL %.3f V, ~%.1f mA nominal (calculated), when enabled\n", code,
                static_cast<double>(ad5241::stim_level_volts(code)),
                static_cast<double>(nominal_ma_for_code(code)));
    std::printf("OUTPUT_ENABLE is %s\n",
                output_enabled() ? "HIGH (SHDN released)"
                                 : "low: SHDN holds the wiper at 0 V, STIM_LEVEL ~0 V");
    return 0;
}

#if NMT_BENCH_STIM
// ---- Commands: stimulation (opt-in build only) ------------------------------

bool parse_code_and_width(int argc, char** argv, std::uint8_t* code, std::uint32_t* width) {
    long c = 0;
    long w = 0;
    if (argc != 3 || !parse_long(argv[1], 0, kBenchMaxPulseCode, &c) ||
        !parse_long(argv[2], hal::kPulseWidthMinUs, hal::kPulseWidthMaxUs, &w)) {
        std::printf("usage: %s <pot code 0-%u> <width us %" PRIu32 "-%" PRIu32 ">\n", argv[0],
                    kBenchMaxPulseCode, hal::kPulseWidthMinUs, hal::kPulseWidthMaxUs);
        std::printf("(code capped at %u, ~%.1f mA nominal, until a measured table exists)\n",
                    kBenchMaxPulseCode,
                    static_cast<double>(nominal_ma_for_code(kBenchMaxPulseCode)));
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

// One single-shot conversion of IMP_SENSE, optionally with a pulse fired
// inside the conversion window. ADR-0014: the pulse is shorter than a
// conversion, so the result is an average; it is turned into ohms through a
// table measured against known resistors.
esp_err_t imp_conversion(bool with_pulse, std::uint32_t width, std::int32_t* code) {
    Config c;
    c.mux = single_ended(kImpedanceAdcInput);
    c.vref = Vref::Internal2048;
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
    if (!adc_ready() || !parse_code_and_width(argc, argv, &code, &width) || !stimulator_ready()) {
        return 1;
    }
    const bool was = pause_emg();
    std::int32_t base = 0;
    std::int32_t pulsed = 0;
    esp_err_t err = imp_conversion(false, width, &base);
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
    const float v_base = code_to_volts(base, kInternalRefV);
    const float v_pulse = code_to_volts(pulsed, kInternalRefV);
    std::printf("pot %u, %" PRIu32 " us\n", code, width);
    std::printf("  no pulse   code %8" PRId32 "  %.5f V at AIN%d\n", base,
                static_cast<double>(v_base), kImpedanceAdcInput);
    std::printf("  with pulse code %8" PRId32 "  %.5f V at AIN%d\n", pulsed,
                static_cast<double>(v_pulse), kImpedanceAdcInput);
    std::printf("  difference %.5f V at AIN%d = %.3f V averaged at STIM_OUT\n",
                static_cast<double>(v_pulse - v_base), kImpedanceAdcInput,
                static_cast<double>((v_pulse - v_base) / kStimOutDivider));
    std::printf("  (an average over the conversion, not the pulse voltage: log it against the\n"
                "   load resistor to build the ADR-0014 table)\n");
    return 0;
}

int cmd_clear_fault(int, char**) {
    g_stim->clear_fault();
    std::printf("stimulator fault cleared\n");
    return 0;
}
#endif  // NMT_BENCH_STIM

// ---- Commands: status and recovery ------------------------------------------

int cmd_status(int, char**) {
#if NMT_BENCH_STIM
    std::printf("mode: STIMULATING BENCH BUILD (dummy loads only)\n");
    std::printf("stimulator: %s, output %s, pot code %u\n", g_stim->faulted() ? "FAULTED" : "ok",
                g_stim->output_enabled() ? "ENABLED" : "disabled", g_stim->amplitude_code());
#else
    std::printf("mode: NO-STIM bench build. PULSE GPIO%d and OUTPUT_ENABLE GPIO%d held low;\n"
                "      no pulse generator linked.\n",
                kStimPulseGpio, kStimOutputEnableGpio);
#endif
    std::printf("ADC: %s, SCL %" PRIu32 " Hz\n", g_adc_ready ? "ready" : "not reset yet",
                g_adc->scl_hz());
    std::printf("EMG: %s%s\n", g_emg->running() ? "running" : "stopped",
                g_emg->error() ? ", ERROR (bus)" : "");
    std::printf("HV rails: ALWAYS ON whenever the shield is powered; firmware cannot switch them\n");
    return 0;
}

int cmd_bus_reset(int argc, char** argv) {
    const bool board = argc == 2 && std::strcmp(argv[1], "board") == 0;
    const esp_err_t err = i2c_master_bus_reset(board ? g_board_bus : g_shield_bus);
    std::printf("%s bus reset: %s\n", board ? "board" : "shield", esp_err_to_name(err));
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
    register_command("scan", "Address-only scan of the board and/or shield bus",
                     "[board|shield]", cmd_scan);
    register_command("adc_reset", "Reset the ADC and check its registers; enables ADC commands",
                     nullptr, cmd_adc_reset);
    register_command("adc_regs", "Read the ADC's four configuration registers", nullptr,
                     cmd_adc_regs);
    register_command("adc_inputs", "AVDD, AIN0-3 single-ended, AIN0-AIN1, offset", nullptr,
                     cmd_adc_inputs);
    register_command("stim_out_dc", "STIM_OUT's DC level via IMP_SENSE (no pulse)", nullptr,
                     cmd_stim_out_dc);
    register_command("emg_start", "Start 2000 SPS EMG sampling on AIN0-AIN1", nullptr,
                     cmd_emg_start);
    register_command("emg_stop", "Stop EMG sampling", nullptr, cmd_emg_stop);
    register_command("emg_stats", "Sample rate, missed/duplicate/clipped counts, noise", nullptr,
                     cmd_emg_stats);
    register_command("emg_dump", "Print recent samples as CSV (index,mv)", "[samples]",
                     cmd_emg_dump);
    register_command("pot", "Read, or write and verify, the amplitude pot", "[code]", cmd_pot);
#if NMT_BENCH_STIM
    register_command("pulse", "One pulse: set pot, enable, fire, disable", "<code> <width_us>",
                     cmd_pulse);
    register_command("train", "Four pulses 500 ms apart, output enabled only for the train",
                     "<code> <width_us>", cmd_train);
    register_command("imp", "IMP_SENSE single-shot with and without a pulse (ADR-0014)",
                     "<code> <width_us>", cmd_imp);
    register_command("clear_fault", "Clear a latched stimulator fault", nullptr, cmd_clear_fault);
#endif
    register_command("status", "Build mode, ADC, EMG and stimulator state", nullptr, cmd_status);
    register_command("bus_reset", "Clock out a stuck I2C bus (shield by default)",
                     "[board|shield]", cmd_bus_reset);
}

}  // namespace

extern "C" void app_main() {
    // Claim the stimulator lines low before anything else can fail.
#if NMT_BENCH_STIM
    g_pulse = new PulseGenerator(kStimPulseGpio);
    const esp_err_t pulse_err = g_pulse->init();
    hold_low(kStimOutputEnableGpio);  // ShieldStimulator::init takes it over below
#else
    hold_low(kStimPulseGpio);
    hold_low(kStimOutputEnableGpio);
#endif

    // Two buses (ADR-0015). The bench does not link the BSP, so it creates the
    // board bus itself on the BSP's port and pins, for scanning only.
    g_board_bus = new_bus(kBoardI2cPort, kBoardI2cSdaGpio, kBoardI2cSclGpio, false);
    // Wiring check first, while the pins are still plain GPIO.
    check_shield_lines();
    // Internal pull-ups only so an unwired bus does not float; the shield's
    // R2/R4 (3.3 kOhm) are the real ones.
    g_shield_bus = new_bus(kShieldI2cPort, kShieldI2cSdaGpio, kShieldI2cSclGpio, true);

    // Adding a device sends nothing on the wire.
    g_adc = new Ads122c04(g_shield_bus, kAdcAddress, kI2cBringupHz);
    g_pot = new Ad5241(g_shield_bus, kPotAddress, kI2cBringupHz);
    const esp_err_t adc_err = g_adc->init();
    const esp_err_t pot_err = g_pot->init();
    g_emg = new ShieldEmg(*g_adc, g_shield_bus, kEmgMux, kFrontEndGain);

    std::printf("\n==============================================================\n");
#if NMT_BENCH_STIM
    std::printf(" NMT shield bench firmware: STIMULATING BUILD\n"
                " Resistive dummy loads on CN1 only, never a person.\n"
                " Pulse codes capped at %u (~%.1f mA nominal).\n",
                kBenchMaxPulseCode, static_cast<double>(nominal_ma_for_code(kBenchMaxPulseCode)));
#else
    std::printf(" NMT shield bench firmware: NO-STIM BUILD (output disabled)\n"
                " STIM_PULSE and STIM_OUTPUT_ENABLE held low; no pulse commands.\n");
#endif
    std::printf(" HV rails are always on while the shield is powered.\n");
    std::printf("==============================================================\n");
    std::printf("reset reason: %s\n", reset_reason_name(esp_reset_reason()));
    std::printf("pins: STIM_PULSE GPIO%d (J1 35), STIM_OUTPUT_ENABLE GPIO%d (J1 37)\n",
                kStimPulseGpio, kStimOutputEnableGpio);
    std::printf("      shield I2C SDA GPIO%d, SCL GPIO%d, %" PRIu32 " Hz for bring-up\n",
                kShieldI2cSdaGpio, kShieldI2cSclGpio, kI2cBringupHz);
    std::printf("ADS122C04 add: %s   AD5241 add: %s\n", esp_err_to_name(adc_err),
                esp_err_to_name(pot_err));

#if NMT_BENCH_STIM
    std::printf("pulse generator: %s\n", esp_err_to_name(pulse_err));
    // Writes pot code 0 with readback (the AD5241 powers up at midscale).
    g_stim = new ShieldStimulator(*g_pot, *g_pulse, kStimOutputEnableGpio, AmplitudeTable{});
    const esp_err_t stim_err = g_stim->init();
    std::printf("stimulator init: %s%s\n", esp_err_to_name(stim_err),
                stim_err != ESP_OK ? " (stimulator faulted; check `scan`)" : "");
#endif

    std::printf("\n");
    print_shield_lines();
    scan_board_bus();
    scan_shield_bus();
    std::printf("\nType `help`. Next: `adc_reset`.\n\n");

    esp_console_repl_t* repl = nullptr;
    esp_console_repl_config_t rc = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    rc.prompt = "bench>";
    esp_console_dev_uart_config_t uc = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uc, &rc, &repl));
    esp_console_register_help_command();
    register_commands();
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
