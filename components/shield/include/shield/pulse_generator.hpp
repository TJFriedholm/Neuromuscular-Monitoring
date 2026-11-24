// Hardware one-shot on the STIM_PULSE line (ESP-IDF only). ADR-0001, ADR-0012.
//
// Embedded note: the ESP32-P4's RMT peripheral plays back a list of
// (level, duration) pairs on a pin by itself, timed by its own clock. We give
// it one pair -- "high for width_us" -- followed by "low", and tell it to play
// once. After rmt_transmit() returns, the CPU has no further part in the pulse:
// if the CPU hangs, the pulse still ends on time. The CPU never writes the pin.

#ifndef NMT_SHIELD_PULSE_GENERATOR_HPP
#define NMT_SHIELD_PULSE_GENERATOR_HPP

#include <cstdint>

#include "driver/rmt_tx.h"
#include "esp_err.h"

namespace shield {

class PulseGenerator {
  public:
    explicit PulseGenerator(int gpio);
    ~PulseGenerator();
    PulseGenerator(const PulseGenerator&) = delete;
    PulseGenerator& operator=(const PulseGenerator&) = delete;

    // Claims the pin with the RMT idle-low. Call as early in boot as possible.
    esp_err_t init();

    // Plays one high pulse of width_us and waits (<= 5 ms) for it to finish.
    // Widths outside the spec's 200-300 us are refused here as well as in the
    // stimulator, so this class cannot be misused on its own.
    //
    // If the pulse has not finished in time, the RMT channel is disabled, the
    // pin is taken back from the RMT and driven low by plain GPIO, and the
    // generator latches: every later fire() fails until a reset. The caller
    // (ShieldStimulator) also trips, which drops OUTPUT_ENABLE, forcing the
    // command to 0 V even if this pin were stuck high (ADR-0011).
    esp_err_t fire(std::uint32_t width_us);
    bool failed() const { return failed_; }

  private:
    void force_low();

    int gpio_;
    bool failed_ = false;
    // The RMT reads the pulse from memory during the transmission, so it lives
    // here, not on fire()'s stack.
    rmt_symbol_word_t symbol_{};
    rmt_channel_handle_t channel_ = nullptr;
    rmt_encoder_handle_t encoder_ = nullptr;
};

}  // namespace shield

#endif  // NMT_SHIELD_PULSE_GENERATOR_HPP
