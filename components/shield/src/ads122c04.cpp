#include "shield/ads122c04.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "shield/board.hpp"

namespace shield {

using namespace ads122c04;

Ads122c04::Ads122c04(i2c_master_bus_handle_t bus, std::uint8_t address, std::uint32_t scl_hz)
    : bus_(bus), address_(address), scl_hz_(scl_hz) {}

Ads122c04::~Ads122c04() {
    if (dev_ != nullptr) {
        i2c_master_bus_rm_device(dev_);
    }
}

esp_err_t Ads122c04::init() {
    i2c_device_config_t dc{};
    dc.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dc.device_address = address_;
    dc.scl_speed_hz = scl_hz_;
    esp_err_t err = i2c_master_bus_add_device(bus_, &dc, &dev_);
    if (err != ESP_OK) {
        return err;
    }
    return reset();
}

esp_err_t Ads122c04::write_locked(const std::uint8_t* bytes, std::size_t n) {
    return i2c_master_transmit(dev_, bytes, n, kI2cTimeoutMs);
}

esp_err_t Ads122c04::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::uint8_t cmd = kCmdReset;
    const esp_err_t err = write_locked(&cmd, 1);
    // The datasheet asks for a short wait (tens of us) after RESET; 2 ms is
    // generous and this is not on any time-critical path.
    vTaskDelay(pdMS_TO_TICKS(2));
    cfg_ = Config{};
    return err;
}

esp_err_t Ads122c04::powerdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::uint8_t cmd = kCmdPowerdown;
    return write_locked(&cmd, 1);
}

esp_err_t Ads122c04::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::uint8_t cmd = kCmdStartSync;
    return write_locked(&cmd, 1);
}

esp_err_t Ads122c04::write_register(std::uint8_t reg, std::uint8_t value) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::uint8_t bytes[2] = {cmd_wreg(reg), value};
    return write_locked(bytes, 2);
}

esp_err_t Ads122c04::read_register_locked(std::uint8_t reg, std::uint8_t* value) {
    const std::uint8_t cmd = cmd_rreg(reg);
    return i2c_master_transmit_receive(dev_, &cmd, 1, value, 1, kI2cTimeoutMs);
}

esp_err_t Ads122c04::read_register(std::uint8_t reg, std::uint8_t* value) {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_register_locked(reg, value);
}

esp_err_t Ads122c04::configure_locked(const Config& cfg) {
    if (!valid(cfg)) {
        return ESP_ERR_INVALID_ARG;
    }
    const std::uint8_t want[4] = {reg0(cfg), reg1(cfg), reg2(cfg), reg3(cfg)};
    for (std::uint8_t r = 0; r < 4; ++r) {
        const std::uint8_t bytes[2] = {cmd_wreg(r), want[r]};
        const esp_err_t err = write_locked(bytes, 2);
        if (err != ESP_OK) {
            return err;
        }
    }
    for (std::uint8_t r = 0; r < 4; ++r) {
        std::uint8_t got = 0;
        const esp_err_t err = read_register_locked(r, &got);
        if (err != ESP_OK) {
            return err;
        }
        const std::uint8_t mask = (r == 2) ? static_cast<std::uint8_t>(~kReg2DrdyBit) : 0xFF;
        if ((got & mask) != (want[r] & mask)) {
            return ESP_ERR_INVALID_RESPONSE;
        }
    }
    cfg_ = cfg;
    return ESP_OK;
}

esp_err_t Ads122c04::configure(const Config& cfg) {
    std::lock_guard<std::mutex> lock(mutex_);
    return configure_locked(cfg);
}

esp_err_t Ads122c04::read_data(std::int32_t* code, std::uint8_t* counter) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::uint8_t cmd = kCmdRdata;
    std::uint8_t buf[4] = {};
    if (cfg_.data_counter) {
        const esp_err_t err = i2c_master_transmit_receive(dev_, &cmd, 1, buf, 4, kI2cTimeoutMs);
        if (err != ESP_OK) {
            return err;
        }
        *counter = buf[0];
        *code = sign_extend_24(buf[1], buf[2], buf[3]);
        return ESP_OK;
    }
    const esp_err_t err = i2c_master_transmit_receive(dev_, &cmd, 1, buf, 3, kI2cTimeoutMs);
    if (err != ESP_OK) {
        return err;
    }
    *code = sign_extend_24(buf[0], buf[1], buf[2]);
    return ESP_OK;
}

esp_err_t Ads122c04::read_single(Mux mux, Vref vref, std::int32_t* code) {
    std::lock_guard<std::mutex> lock(mutex_);
    Config c;
    c.mux = mux;
    c.vref = vref;
    c.rate = DataRate::Dr20;  // slowest, quietest: these are DC readings
    c.turbo = false;
    c.continuous = false;
    c.data_counter = false;
    esp_err_t err = configure_locked(c);
    if (err != ESP_OK) {
        return err;
    }
    const std::uint8_t start = kCmdStartSync;
    err = write_locked(&start, 1);
    if (err != ESP_OK) {
        return err;
    }
    // 20 SPS single-shot takes ~50 ms; allow 200.
    for (int i = 0; i < 200; ++i) {
        vTaskDelay(pdMS_TO_TICKS(1));
        std::uint8_t r2 = 0;
        err = read_register_locked(2, &r2);
        if (err != ESP_OK) {
            return err;
        }
        if (r2 & kReg2DrdyBit) {
            const std::uint8_t rdata = kCmdRdata;
            std::uint8_t buf[3] = {};
            err = i2c_master_transmit_receive(dev_, &rdata, 1, buf, 3, kI2cTimeoutMs);
            if (err == ESP_OK) {
                *code = sign_extend_24(buf[0], buf[1], buf[2]);
            }
            return err;
        }
    }
    return ESP_ERR_TIMEOUT;
}

}  // namespace shield
