// EMG acquisition interface.
//
// On shield V1.0 (ADR-0010) this is an ADS122C04 (24-bit, I2C) behind an
// analog front end of nominal gain 50.2 V/V. It runs at 2000 SPS and is polled,
// as its DRDY pin is not connected (ADR-0014). §5.1.2 requires >= 1000 Hz.

#ifndef NMT_HAL_EMG_HPP
#define NMT_HAL_EMG_HPP

#include <cstddef>

namespace hal {

class IEmg {
  public:
    virtual ~IEmg() = default;

    virtual bool start() = 0;
    virtual bool stop() = 0;

    // Copies up to max_samples of the most recent CMAP window into out, in
    // millivolts at the electrodes. Returns the number written, or -1 on error
    // -- including a window with too many missed samples to trust.
    virtual int read_window(float* out, std::size_t max_samples) = 0;

    virtual float sample_rate_hz() const = 0;
};

}  // namespace hal

#endif  // NMT_HAL_EMG_HPP
