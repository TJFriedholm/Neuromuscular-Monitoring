#include "safety/fault.hpp"

namespace safety {

void FaultLatch::raise(FaultCause cause) {
    if (cause == FaultCause::None) {
        return;
    }
    if (!active_) {
        active_ = true;
        cause_ = cause;
    }
    // Already latched: keep the original cause.
}

void FaultLatch::acknowledge() {
    active_ = false;
    cause_ = FaultCause::None;
}

}  // namespace safety
