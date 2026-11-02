// A stand-in for the device, for the desktop simulator and the firmware's
// demo build. Nothing here touches hardware or stimulates anyone.
//
// Drives the real safety::TofStateMachine, calib::run, tof::compute and
// correction::apply with synthetic CMAP data from hal_mock, on a simulated
// clock. What the screen shows is therefore what the core logic decides, not
// values made up for the picture.

#ifndef NMT_SIM_DEVICE_HPP
#define NMT_SIM_DEVICE_HPP

#include <cstddef>
#include <cstdint>

#include "hal_mock/cmap_source.hpp"
#include "safety/tof_state_machine.hpp"
#include "ui/controller.hpp"
#include "ui/menu_model.hpp"

namespace sim {

// Blockade levels the simulator can step through, after the UI mockup's bench
// controls, plus one with T4 > T1 to show the "Suspect" path and one with no
// response at all. Alternating switches between Shallow and Recovered on every
// Train-of-Four (Facial TOF Ratio 0.87, then 0.97), and every 10th train since
// START raises an electrode impedance fault instead, to show the fault alarm.
enum class Blockade { Alternating, Deep, Moderate, Shallow, Recovered, Anomalous, None };

const char* name(Blockade b);

class SimDevice : public ui::Controller {
  public:
    SimDevice();

    // Advances the simulation to `now_ms`. Call once per frame.
    void tick(std::uint32_t now_ms);

    // ui::Controller
    void start() override;
    void stop() override;
    void recalibrate() override;
    void acknowledge_fault() override;
    void set(ui::Field f, float value) override;
    void set_volume(ui::Volume v) override;

    // Bench controls (keyboard).
    void raise_stimulator_fault();
    // Cycles the patient through the Blockade levels; takes effect on the
    // next Train-of-Four.
    Blockade next_blockade();
    // Toggles electrode impedance between in range and out of range. Checked
    // at START.
    bool toggle_impedance();

    ui::Snapshot snapshot() const;

    // The last 3 s of EMG, `n` points oldest first, in microvolts.
    void trace(std::int32_t* out_uv, std::size_t n) const;

  private:
    void calibrate();
    void begin_train();
    void reset_measurement();

    safety::TofStateMachine sm_;
    hal_mock::CmapParams cmap_;
    ui::Settings settings_;
    Blockade blockade_ = Blockade::Alternating;
    // Which half of Alternating the next train uses.
    bool alternate_recovered_ = false;
    // Trains begun since START, for Alternating's fault on every 10th.
    int trains_since_start_ = 0;
    float impedance_ohm_;
    float operating_current_ma_ = 0.0f;

    std::uint32_t now_ms_ = 0;
    std::uint32_t state_since_ms_ = 0;
    std::uint32_t next_train_ms_ = 0;

    float train_mv_[tof::kTwitchCount] = {};
    bool has_measurement_ = false;
    float amps_mv_[tof::kTwitchCount] = {};
    tof::Result tof_{};
    correction::Result correction_{};

    // Recent stimuli, for drawing the EMG strip.
    struct Pulse {
        std::uint32_t at_ms;
        float amp_mv;
    };
    static constexpr int kPulseHistory = 8;
    Pulse pulses_[kPulseHistory] = {};
    int pulse_head_ = 0;
};

}  // namespace sim

#endif  // NMT_SIM_DEVICE_HPP
