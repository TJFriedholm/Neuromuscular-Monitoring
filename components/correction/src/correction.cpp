#include "correction/correction.hpp"

#include <cmath>
#include <limits>

namespace correction {

namespace {
constexpr float kNan = std::numeric_limits<float>::quiet_NaN();
}

bool exponent_in_range(float exponent) {
    return std::isfinite(exponent) && exponent >= kExponentMin && exponent <= kExponentMax;
}

float facial_needed_for(float target, float exponent) {
    if (!exponent_in_range(exponent) || !std::isfinite(target) || target < 0.0f) {
        return kNan;
    }
    // Inverse of f(x) = x^n.
    return std::pow(target, 1.0f / exponent);
}

Result apply(float facial_ratio, float exponent) {
    Result out{};
    out.peripheral_ratio = kNan;

    if (!std::isfinite(facial_ratio) || facial_ratio < 0.0f) {
        out.status = Status::InvalidInput;
        return out;
    }
    if (!std::isfinite(exponent)) {
        out.status = Status::NotConfigured;
        return out;
    }
    if (!exponent_in_range(exponent)) {
        out.status = Status::InvalidExponent;
        return out;
    }
    if (facial_ratio > 1.0f) {
        out.status = Status::AnomalousInput;
        return out;
    }

    out.status = Status::Ok;
    out.peripheral_ratio = std::pow(facial_ratio, exponent);
    return out;
}

}  // namespace correction
