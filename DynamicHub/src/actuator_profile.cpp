#include "dynamic_hub/actuator_profile.hpp"

namespace dynamic_hub {

RigProfile default_profile() {
    RigProfile profile{};
    for (size_t i = 0; i < profile.actuators.size(); ++i) {
        profile.actuators[i].name = "Actuator " + std::to_string(i + 1);
        profile.actuators[i].min_height_mm = 0.0;
        profile.actuators[i].max_height_mm = 500.0;
        profile.actuators[i].home_height_mm = 10.0;
        profile.actuators[i].max_speed_mm_s = 12000.0;
        profile.actuators[i].offset_mm = 0.0;

        profile.blends[i].heave_mix = 1.0;
        profile.blends[i].sway_mix = (i % 2 == 0) ? -0.5 : 0.5;
        profile.blends[i].surge_mix = (i < 2) ? 0.5 : -0.5;
        profile.blends[i].roll_mix = (i % 2 == 0) ? -0.5 : 0.5;
        profile.blends[i].pitch_mix = (i < 2) ? -0.5 : 0.5;
        profile.blends[i].yaw_mix = 0.0;
    }
    return profile;
}

}  // namespace dynamic_hub
