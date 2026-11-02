// EMG acquisition interface (ADS1299-4PAG, SPI, per §5.6.1).

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
    // millivolts. Returns the number written, or -1 on error.
    virtual int read_window(float* out, std::size_t max_samples) = 0;

    virtual float sample_rate_hz() const = 0;
};

}  // namespace hal

#endif  // NMT_HAL_EMG_HPP
