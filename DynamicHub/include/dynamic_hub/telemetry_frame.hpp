#pragma once

#include <cstdint>
#include <array>

namespace dynamic_hub {

struct TelemetryFrame {
    double surge_mps2{0.0};
    double sway_mps2{0.0};
    double heave_mps2{0.0};
    double roll_rad{0.0};
    double pitch_rad{0.0};
    double yaw_rad{0.0};
};

using ActuatorTargets = std::array<double, 4>;

}  // namespace dynamic_hub
