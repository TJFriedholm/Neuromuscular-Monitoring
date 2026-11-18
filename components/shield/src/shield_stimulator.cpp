#include "shield/shield_stimulator.hpp"

#include <utility>

#include "driver/gpio.h"
#include "esp_rom_sys.h"

namespace shield {

namespace {
// Time for the AD5241 to leave shutdown and the wiper to settle onto
// STIM_LEVEL before the first pulse. Generous; the part needs microseconds.
constexpr std::uint32_t kEnableSettleUs = 100;
}  // namespace

ShieldStimulator::ShieldStimulator(Ad5241& pot, PulseGenerator& pulse, int output_enable_gpio,
                                   AmplitudeTable table)
    : pot_(pot), pulse_(pulse), enable_gpio_(output_enable_gpio), table_(std::move(table)) {}

esp_err_t ShieldStimulator::init() {
    const auto pin = static_cast<gpio_num_t>(enable_gpio_);
    // Level first, then direction, so the pin never drives high even briefly.
    gpio_set_level(pin, 0);
    gpio_config_t gc{};
    gc.pin_bit_mask = 1ULL << enable_gpio_;
    gc.mode = GPIO_MODE_OUTPUT;
    gc.pull_down_en = GPIO_PULLDOWN_ENABLE;
    esp_err_t err = gpio_config(&gc);
    if (err != ESP_OK) {
        return err;
    }
    gpio_set_level(pin, 0);
    output_on_ = false;

    err = pot_.write_verified(0);
    if (err != ESP_OK) {
        trip();
        return err;
    }
    code_ = 0;
    return ESP_OK;
}

void ShieldStimulator::trip() {
    faulted_ = true;
    gpio_set_level(static_cast<gpio_num_t>(enable_gpio_), 0);
    output_on_ = false;
}

bool ShieldStimulator::set_amplitude_code(std::uint8_t code) {
    if (faulted_) {
        return false;
    }
    if (pot_.write_verified(code) != ESP_OK) {
        trip();
        return false;
    }
    code_ = code;
    return true;
}

bool ShieldStimulator::set_amplitude_ma(float ma) {
    if (faulted_) {
        return false;
    }
    if (!(ma >= hal::kCurrentMinMa && ma <= hal::kCurrentMaxMa)) {
        return false;
    }
    std::uint8_t code = 0;
    if (!table_.code_for_ma(ma, &code)) {
        return false;
    }
    return set_amplitude_code(code);
}

bool ShieldStimulator::enable_output(bool on) {
    const auto pin = static_cast<gpio_num_t>(enable_gpio_);
    if (on) {
        if (faulted_) {
            return false;
        }
        gpio_set_level(pin, 1);
        output_on_ = true;
        esp_rom_delay_us(kEnableSettleUs);
        return true;
    }
    // Off: the GPIO write is what removes the command; do it before anything
    // that can fail.
    gpio_set_level(pin, 0);
    output_on_ = false;
    if (pot_.write_verified(0) != ESP_OK) {
        trip();
        return false;
    }
    code_ = 0;
    return true;
}

bool ShieldStimulator::fire_pulse(std::uint32_t width_us) {
    if (faulted_ || !output_on_) {
        return false;
    }
    if (width_us < hal::kPulseWidthMinUs || width_us > hal::kPulseWidthMaxUs) {
        return false;
    }
    if (pulse_.fire(width_us) != ESP_OK) {
        trip();
        return false;
    }
    return true;
}

}  // namespace shield
