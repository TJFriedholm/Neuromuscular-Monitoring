// What the UI may ask the device to do.
//
// The UI never changes device state itself. It calls these, and the device
// decides: it validates every value again and may refuse. The device's new
// state reaches the screen through the next Snapshot.

#ifndef NMT_UI_CONTROLLER_HPP
#define NMT_UI_CONTROLLER_HPP

#include "ui/settings.hpp"

namespace ui {

class Controller {
  public:
    virtual ~Controller() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    // Stop, then run Supramaximal Calibration again.
    virtual void recalibrate() = 0;
    virtual void acknowledge_fault() = 0;

    virtual void set(Field f, float value) = 0;
    virtual void set_volume(Volume v) = 0;
};

}  // namespace ui

#endif  // NMT_UI_CONTROLLER_HPP
