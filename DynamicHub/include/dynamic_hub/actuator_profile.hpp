#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dynamic_hub {

struct ActuatorConfig {
    std::string name;
    double min_height_mm{0.0};
    double max_height_mm{300.0};
    double home_height_mm{10.0};
    double max_speed_mm_s{150.0};
    double offset_mm{0.0};
};

struct AxisBlend {
    double surge_mix{0.0};
    double sway_mix{0.0};
    double heave_mix{1.0};
    double roll_mix{0.0};
    double pitch_mix{0.0};
    double yaw_mix{0.0};
};

struct RigProfile {
    std::array<ActuatorConfig, 4> actuators;
    std::array<AxisBlend, 4> blends;
};

RigProfile default_profile();

}  // namespace dynamic_hub
