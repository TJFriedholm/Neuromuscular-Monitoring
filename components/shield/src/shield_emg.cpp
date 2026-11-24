#include "shield/shield_emg.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "esp_timer.h"
#include "shield/sample_math.hpp"

namespace shield {

using namespace ads122c04;

namespace {
constexpr std::size_t kRingSamples = 8192;  // ~4 s at 2000 SPS
constexpr std::uint32_t kTimerHz = 1000000;
constexpr std::uint32_t kPollPeriodUs = 400;  // 2500 Hz, above the ADC's 2000
constexpr float kNominalSps = 2000.0f;
// AVDD is ISO_5V. Outside this, the reference (and every mV value) is wrong.
constexpr float kAvddMinV = 4.5f;
constexpr float kAvddMaxV = 5.5f;
}  // namespace

ShieldEmg::ShieldEmg(Ads122c04& adc, i2c_master_bus_handle_t bus, Mux mux, float front_end_gain)
    : adc_(adc), bus_(bus), mux_(mux), gain_(front_end_gain), ring_(kRingSamples, 0.0f) {
    exited_ = xSemaphoreCreateBinary();
}

ShieldEmg::~ShieldEmg() {
    stop();
    if (exited_ != nullptr) {
        vSemaphoreDelete(exited_);
    }
}

bool ShieldEmg::on_alarm(gptimer_handle_t, const gptimer_alarm_event_data_t*, void* arg) {
    auto* self = static_cast<ShieldEmg*>(arg);
    BaseType_t woken = pdFALSE;
    if (self->task_ != nullptr) {
        vTaskNotifyGiveFromISR(self->task_, &woken);
    }
    return woken == pdTRUE;
}

void ShieldEmg::task_entry(void* arg) {
    static_cast<ShieldEmg*>(arg)->run();
}

void ShieldEmg::push(float mv) {
    // Caller holds mutex_.
    const std::uint32_t t = total_;
    ring_[t % kRingSamples] = mv;
    total_ = t + 1;
}

void ShieldEmg::run() {
    bool have_last = false;
    std::uint8_t last = 0;
    int consecutive_errors = 0;
    std::int64_t window_start = esp_timer_get_time();
    std::uint32_t window_fresh = 0;

    while (running_) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
        if (!running_) {
            break;
        }
        if (error_) {
            continue;  // stay alive until stop(); the timer still points here
        }

        std::int32_t code = 0;
        std::uint8_t counter = 0;
        const esp_err_t err = adc_.read_data(&code, &counter);

        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.polls;
        if (err != ESP_OK) {
            ++stats_.bus_errors;
            if (++consecutive_errors >= 2) {
                error_ = true;
            } else {
                i2c_master_bus_reset(bus_);
            }
            continue;
        }
        consecutive_errors = 0;

        int advance = 1;
        if (have_last) {
            advance = counter_advance(last, counter);
        }
        have_last = true;
        last = counter;

        if (advance == 0) {
            ++stats_.duplicates;
            continue;
        }
        for (int i = 1; i < advance; ++i) {
            push(std::numeric_limits<float>::quiet_NaN());
        }
        stats_.missed += static_cast<std::uint32_t>(advance - 1);
        if (is_clipped(code)) {
            ++stats_.clipped;
        }
        push(electrode_mv(code, vref_v_, gain_));
        ++stats_.fresh;
        ++window_fresh;

        const std::int64_t now = esp_timer_get_time();
        if (now - window_start >= 1000000) {
            stats_.measured_sps =
                static_cast<float>(window_fresh) * 1e6f / static_cast<float>(now - window_start);
            window_start = now;
            window_fresh = 0;
        }
    }
    xSemaphoreGive(exited_);
    vTaskDelete(nullptr);
}

bool ShieldEmg::start() {
    if (running_) {
        return true;
    }
    error_ = false;
    start_error_ = EmgStartError::None;

    if (!polling_fits(adc_.scl_hz(), kPollPeriodUs)) {
        start_error_ = EmgStartError::BusTooSlow;
        return false;
    }

    // AVDD (ISO_5V) powers the front end and sets ISO_VCM. Out of range means
    // the analog side is not working, whatever the ADC reads.
    std::int32_t monitor = 0;
    if (adc_.read_single(Mux::AvddMonitor, Vref::Internal2048, &monitor) != ESP_OK) {
        error_ = true;
        start_error_ = EmgStartError::AdcNoAnswer;
        return false;
    }
    const float avdd = avdd_from_monitor(monitor);
    if (!(avdd >= kAvddMinV && avdd <= kAvddMaxV)) {
        error_ = true;
        start_error_ = EmgStartError::AvddOutOfRange;
        return false;
    }

    Config c;  // defaults are the EMG configuration
    c.mux = mux_;
    vref_v_ = kInternalRefV;
    if (adc_.configure(c) != ESP_OK || adc_.start() != ESP_OK) {
        error_ = true;
        start_error_ = EmgStartError::ConfigFailed;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        total_ = 0;
        stats_ = EmgStats{};
        stats_.avdd_v = avdd;
        stats_.vref_v = vref_v_;
    }

    running_ = true;
    if (xTaskCreatePinnedToCore(task_entry, "emg", 4096, this, configMAX_PRIORITIES - 2, &task_,
                                1) != pdPASS) {
        running_ = false;
        task_ = nullptr;
        start_error_ = EmgStartError::NoResources;
        return false;
    }

    gptimer_config_t tc{};
    tc.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    tc.direction = GPTIMER_COUNT_UP;
    tc.resolution_hz = kTimerHz;
    gptimer_event_callbacks_t cbs{};
    cbs.on_alarm = on_alarm;
    gptimer_alarm_config_t ac{};
    ac.alarm_count = kPollPeriodUs;
    ac.reload_count = 0;
    ac.flags.auto_reload_on_alarm = true;
    if (gptimer_new_timer(&tc, &timer_) != ESP_OK ||
        gptimer_register_event_callbacks(timer_, &cbs, this) != ESP_OK ||
        gptimer_set_alarm_action(timer_, &ac) != ESP_OK || gptimer_enable(timer_) != ESP_OK ||
        gptimer_start(timer_) != ESP_OK) {
        stop();
        error_ = true;
        start_error_ = EmgStartError::NoResources;
        return false;
    }
    return true;
}

bool ShieldEmg::stop() {
    // Timer first, so nothing notifies the task after it has gone.
    if (timer_ != nullptr) {
        gptimer_stop(timer_);
        gptimer_disable(timer_);
        gptimer_del_timer(timer_);
        timer_ = nullptr;
    }
    if (task_ != nullptr) {
        running_ = false;
        xTaskNotifyGive(task_);
        xSemaphoreTake(exited_, pdMS_TO_TICKS(100));
        task_ = nullptr;
    }
    running_ = false;
    adc_.powerdown();
    return true;
}

std::size_t ShieldEmg::copy_recent(float* out, std::size_t max_samples) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::uint32_t total = total_;
    const std::size_t n =
        std::min<std::size_t>({max_samples, static_cast<std::size_t>(total), kRingSamples});
    const std::uint32_t first = total - static_cast<std::uint32_t>(n);
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = ring_[(first + i) % kRingSamples];
    }
    return n;
}

int ShieldEmg::read_window(float* out, std::size_t max_samples) {
    if (!running_ || error_) {
        return -1;
    }
    const std::size_t n = copy_recent(out, max_samples);
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(out[i])) {
            return -1;
        }
    }
    return static_cast<int>(n);
}

float ShieldEmg::sample_rate_hz() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_.measured_sps > 0.0f ? stats_.measured_sps : kNominalSps;
}

EmgStats ShieldEmg::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

}  // namespace shield
