// ADS122C04 driver: 24-bit ADC on the shared I2C bus (ESP-IDF only).
//
// Thin: it sends commands and moves bytes. Sampling at a fixed rate is
// ShieldEmg's job. Calls are serialised by an internal mutex, so a
// configure-then-read sequence is not interleaved with another task's.

#ifndef NMT_SHIELD_ADS122C04_HPP
#define NMT_SHIELD_ADS122C04_HPP

#include <cstdint>
#include <mutex>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "shield/ads122c04_regs.hpp"

namespace shield {

class Ads122c04 {
  public:
    Ads122c04(i2c_master_bus_handle_t bus, std::uint8_t address, std::uint32_t scl_hz);
    ~Ads122c04();
    Ads122c04(const Ads122c04&) = delete;
    Ads122c04& operator=(const Ads122c04&) = delete;

    // Adds the device to the bus and resets it.
    esp_err_t init();

    esp_err_t reset();
    esp_err_t powerdown();
    // Starts a single-shot conversion, or continuous conversions.
    esp_err_t start();

    // Writes all four registers and reads them back. ESP_ERR_INVALID_RESPONSE
    // if the readback differs (a wiring or bus fault).
    esp_err_t configure(const ads122c04::Config& cfg);
    const ads122c04::Config& config() const { return cfg_; }

    esp_err_t read_register(std::uint8_t reg, std::uint8_t* value);
    esp_err_t write_register(std::uint8_t reg, std::uint8_t value);

    // One RDATA transaction. With the data counter enabled, *counter receives
    // it; otherwise *counter is left alone.
    esp_err_t read_data(std::int32_t* code, std::uint8_t* counter);

    // Convenience for occasional readings: single-shot conversion on `mux`
    // with `vref`, waits for DRDY (by polling register 2), returns the code.
    // Leaves the chip configured for single-shot; call configure() again
    // before resuming continuous sampling.
    esp_err_t read_single(ads122c04::Mux mux, ads122c04::Vref vref, std::int32_t* code);

  private:
    esp_err_t write_locked(const std::uint8_t* bytes, std::size_t n);
    esp_err_t configure_locked(const ads122c04::Config& cfg);
    esp_err_t read_register_locked(std::uint8_t reg, std::uint8_t* value);

    i2c_master_bus_handle_t bus_;
    i2c_master_dev_handle_t dev_ = nullptr;
    std::uint8_t address_;
    std::uint32_t scl_hz_;
    ads122c04::Config cfg_{};
    std::mutex mutex_;
};

}  // namespace shield

#endif  // NMT_SHIELD_ADS122C04_HPP
