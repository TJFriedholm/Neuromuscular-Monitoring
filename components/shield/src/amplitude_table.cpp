#include "shield/amplitude_table.hpp"

#include <cmath>
#include <utility>

namespace shield {

AmplitudeTable::AmplitudeTable(std::vector<AmplitudePoint> points) {
    if (points.size() < 2) {
        return;
    }
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (!(points[i].ma > points[i - 1].ma) || points[i].code <= points[i - 1].code) {
            return;
        }
    }
    points_ = std::move(points);
}

bool AmplitudeTable::code_for_ma(float ma, std::uint8_t* code) const {
    if (!valid() || !(ma >= points_.front().ma && ma <= points_.back().ma)) {
        return false;
    }
    for (std::size_t i = 1; i < points_.size(); ++i) {
        const AmplitudePoint& lo = points_[i - 1];
        const AmplitudePoint& hi = points_[i];
        if (ma <= hi.ma) {
            const float t = (ma - lo.ma) / (hi.ma - lo.ma);
            const float c = static_cast<float>(lo.code) + t * static_cast<float>(hi.code - lo.code);
            *code = static_cast<std::uint8_t>(std::lround(c));
            return true;
        }
    }
    return false;
}

bool AmplitudeTable::ma_for_code(std::uint8_t code, float* ma) const {
    if (!valid() || code < points_.front().code || code > points_.back().code) {
        return false;
    }
    for (std::size_t i = 1; i < points_.size(); ++i) {
        const AmplitudePoint& lo = points_[i - 1];
        const AmplitudePoint& hi = points_[i];
        if (code <= hi.code) {
            const float t = static_cast<float>(code - lo.code) / static_cast<float>(hi.code - lo.code);
            *ma = lo.ma + t * (hi.ma - lo.ma);
            return true;
        }
    }
    return false;
}

}  // namespace shield
