#include "dynamic_hub/telemetry_router.hpp"

#include <algorithm>

namespace dynamic_hub {

namespace {
double clamp(double value, double min, double max) {
    return std::max(min, std::min(max, value));
}
}

ActuatorTargets TelemetryRouter::map(const TelemetryFrame& frame) const {
    ActuatorTargets targets{};
    for (size_t i = 0; i < profile_.actuators.size(); ++i) {
        const auto& a = profile_.actuators[i];
        const auto& blend = profile_.blends[i];
        double mm = a.home_height_mm + a.offset_mm;
        mm += blend.heave_mix * frame.heave_mps2;
        mm += blend.surge_mix * frame.surge_mps2;
        mm += blend.sway_mix * frame.sway_mps2;
        mm += blend.pitch_mix * frame.pitch_rad * 100.0;
        mm += blend.roll_mix * frame.roll_rad * 100.0;
        mm += blend.yaw_mix * frame.yaw_rad * 50.0;
        mm = clamp(mm, a.min_height_mm, a.max_height_mm);
        targets[i] = mm;
    }
    return targets;
}

}  // namespace dynamic_hub
