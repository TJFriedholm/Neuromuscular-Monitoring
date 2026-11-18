// hal::IEmg on shield V1.0 (ESP-IDF only). ADR-0014.
//
// Embedded note: the ADC converts on its own clock at ~2000 SPS, but has no
// "data ready" wire to the ESP32. So a hardware timer interrupts every 400 us
// (2500 Hz, deliberately faster than the ADC) and wakes a high-priority task,
// which reads the latest result. Each result carries the ADC's 8-bit
// conversion counter: if it hasn't moved, the read was a duplicate and is
// dropped; if it moved by more than one, conversions were missed and NaN
// placeholders keep the timeline aligned. Sample spacing therefore follows the
// ADC's clock, not the ESP32's.
//
// A failed read gets one bus recovery attempt; a second consecutive failure
// stops sampling and sets error() (ADR-0013).

#ifndef NMT_SHIELD_SHIELD_EMG_HPP
#define NMT_SHIELD_SHIELD_EMG_HPP

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hal/emg.hpp"
#include "shield/ads122c04.hpp"

namespace shield {

struct EmgStats {
    std::uint32_t polls = 0;
    std::uint32_t fresh = 0;       // new conversions read
    std::uint32_t duplicates = 0;  // polls with no new conversion
    std::uint32_t missed = 0;      // conversions skipped over
    std::uint32_t bus_errors = 0;
    float measured_sps = 0.0f;     // fresh conversions per second, last second
    float vref_v = 0.0f;           // AVDD measured at start()
};

class ShieldEmg final : public hal::IEmg {
  public:
    // `ain` is the single-ended ADC input carrying ADC_SIGNAL (0..3).
    ShieldEmg(Ads122c04& adc, i2c_master_bus_handle_t bus, int ain, float front_end_gain);
    ~ShieldEmg() override;

    // hal::IEmg
    bool start() override;
    bool stop() override;
    // Most recent max_samples, in mV at the electrodes. -1 if not running, on
    // a bus error, or if the window contains a missed sample.
    int read_window(float* out, std::size_t max_samples) override;
    float sample_rate_hz() const override;

    // Like read_window, but missed samples come through as NaN instead of
    // failing the whole window. For bench dumps.
    std::size_t copy_recent(float* out, std::size_t max_samples);

    // Total sample slots so far (fresh + missed). A marker for "now" in the
    // sample stream, e.g. at the moment a pulse is fired.
    std::uint32_t sample_count() const { return total_; }

    bool running() const { return running_; }
    bool error() const { return error_; }
    EmgStats stats() const;
    void set_input(int ain) { ain_ = ain; }

  private:
    static bool on_alarm(gptimer_handle_t, const gptimer_alarm_event_data_t*, void* arg);
    static void task_entry(void* arg);
    void run();
    void push(float mv);

    Ads122c04& adc_;
    i2c_master_bus_handle_t bus_;
    int ain_;
    float gain_;
    float vref_v_ = 5.0f;

    gptimer_handle_t timer_ = nullptr;
    TaskHandle_t task_ = nullptr;
    SemaphoreHandle_t exited_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<bool> error_{false};

    mutable std::mutex mutex_;  // guards ring_, total_ and stats_
    std::vector<float> ring_;
    std::atomic<std::uint32_t> total_{0};
    EmgStats stats_;
};

}  // namespace shield

#endif  // NMT_SHIELD_SHIELD_EMG_HPP
