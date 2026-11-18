// AD5241 driver: the stimulation amplitude potentiometer (ESP-IDF only).

#ifndef NMT_SHIELD_AD5241_HPP
#define NMT_SHIELD_AD5241_HPP

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

namespace shield {

class Ad5241 {
  public:
    Ad5241(i2c_master_bus_handle_t bus, std::uint8_t address, std::uint32_t scl_hz);
    ~Ad5241();
    Ad5241(const Ad5241&) = delete;
    Ad5241& operator=(const Ad5241&) = delete;

    esp_err_t init();

    esp_err_t write(std::uint8_t code);
    esp_err_t read(std::uint8_t* code);

    // Writes, reads back, and returns ESP_ERR_INVALID_RESPONSE on mismatch.
    // The readback is the only stimulator self-check this board allows.
    esp_err_t write_verified(std::uint8_t code);

  private:
    i2c_master_bus_handle_t bus_;
    i2c_master_dev_handle_t dev_ = nullptr;
    std::uint8_t address_;
    std::uint32_t scl_hz_;
};

}  // namespace shield

#endif  // NMT_SHIELD_AD5241_HPP
