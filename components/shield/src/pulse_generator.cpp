#include "shield/pulse_generator.hpp"

#include "hal/stimulator.hpp"

namespace shield {

namespace {
// 1 tick = 1 us. A symbol's duration field is 15 bits, so the longest pulse
// the hardware could ever be asked for is 32.767 ms -- and fire() never asks
// for more than 300 us.
constexpr std::uint32_t kResolutionHz = 1000000;
constexpr int kWaitMs = 5;
}  // namespace

PulseGenerator::PulseGenerator(int gpio) : gpio_(gpio) {}

PulseGenerator::~PulseGenerator() {
    if (channel_ != nullptr) {
        rmt_disable(channel_);
        rmt_del_channel(channel_);
    }
    if (encoder_ != nullptr) {
        rmt_del_encoder(encoder_);
    }
}

esp_err_t PulseGenerator::init() {
    rmt_tx_channel_config_t cc{};
    cc.gpio_num = static_cast<gpio_num_t>(gpio_);
    cc.clk_src = RMT_CLK_SRC_DEFAULT;
    cc.resolution_hz = kResolutionHz;
    cc.mem_block_symbols = 48;  // one channel's memory on the ESP32-P4
    cc.trans_queue_depth = 1;   // never more than one pulse in flight
    cc.flags.init_level = 0;
    esp_err_t err = rmt_new_tx_channel(&cc, &channel_);
    if (err != ESP_OK) {
        return err;
    }
    rmt_copy_encoder_config_t ec{};
    err = rmt_new_copy_encoder(&ec, &encoder_);
    if (err != ESP_OK) {
        return err;
    }
    return rmt_enable(channel_);
}

esp_err_t PulseGenerator::fire(std::uint32_t width_us) {
    if (channel_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (width_us < hal::kPulseWidthMinUs || width_us > hal::kPulseWidthMaxUs) {
        return ESP_ERR_INVALID_ARG;
    }
    rmt_symbol_word_t symbol{};
    symbol.level0 = 1;
    symbol.duration0 = width_us;
    symbol.level1 = 0;
    symbol.duration1 = 10;

    rmt_transmit_config_t tc{};
    tc.loop_count = 0;     // play once. -1 would repeat forever: never use it here.
    tc.flags.eot_level = 0;  // and leave the pin low afterwards
    esp_err_t err = rmt_transmit(channel_, encoder_, &symbol, sizeof symbol, &tc);
    if (err != ESP_OK) {
        return err;
    }
    return rmt_tx_wait_all_done(channel_, kWaitMs);
}

}  // namespace shield
