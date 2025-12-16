#pragma once

#include "actuator_profile.hpp"

#include <string>

namespace dynamic_hub {

class ConfigParser {
  public:
    // Load a rig profile from a simple INI-like file.
    // Lines: [actuatorN] and [blendN] sections (1-4)
    // key=value pairs: min_height, max_height, home_height, max_speed, offset
    // and surge, sway, heave, roll, pitch, yaw for blends.
    static RigProfile load_profile(const std::string& path, bool* loaded = nullptr);
};

}  // namespace dynamic_hub
