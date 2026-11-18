#include "shield/ad5241.hpp"

#include "shield/ad5241_regs.hpp"
#include "shield/board.hpp"

namespace shield {

Ad5241::Ad5241(i2c_master_bus_handle_t bus, std::uint8_t address, std::uint32_t scl_hz)
    : bus_(bus), address_(address), scl_hz_(scl_hz) {}

Ad5241::~Ad5241() {
    if (dev_ != nullptr) {
        i2c_master_bus_rm_device(dev_);
    }
}

esp_err_t Ad5241::init() {
    i2c_device_config_t dc{};
    dc.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dc.device_address = address_;
    dc.scl_speed_hz = scl_hz_;
    return i2c_master_bus_add_device(bus_, &dc, &dev_);
}

esp_err_t Ad5241::write(std::uint8_t code) {
    const std::uint8_t bytes[2] = {ad5241::instruction(), code};
    return i2c_master_transmit(dev_, bytes, 2, kI2cTimeoutMs);
}

esp_err_t Ad5241::read(std::uint8_t* code) {
    return i2c_master_receive(dev_, code, 1, kI2cTimeoutMs);
}

esp_err_t Ad5241::write_verified(std::uint8_t code) {
    esp_err_t err = write(code);
    if (err != ESP_OK) {
        return err;
    }
    std::uint8_t got = 0;
    err = read(&got);
    if (err != ESP_OK) {
        return err;
    }
    return got == code ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

}  // namespace shield
