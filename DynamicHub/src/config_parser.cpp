#include "dynamic_hub/config_parser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace dynamic_hub {
namespace {
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

void apply_value(ActuatorConfig& cfg, const std::string& key, double value) {
    if (key == "min_height") cfg.min_height_mm = value;
    else if (key == "max_height") cfg.max_height_mm = value;
    else if (key == "home_height") cfg.home_height_mm = value;
    else if (key == "max_speed") cfg.max_speed_mm_s = value;
    else if (key == "offset") cfg.offset_mm = value;
}

void apply_value(AxisBlend& blend, const std::string& key, double value) {
    if (key == "surge") blend.surge_mix = value;
    else if (key == "sway") blend.sway_mix = value;
    else if (key == "heave") blend.heave_mix = value;
    else if (key == "roll") blend.roll_mix = value;
    else if (key == "pitch") blend.pitch_mix = value;
    else if (key == "yaw") blend.yaw_mix = value;
}

}  // namespace

RigProfile ConfigParser::load_profile(const std::string& path, bool* loaded) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (loaded) *loaded = false;
        return default_profile();
    }

    RigProfile profile = default_profile();
    std::string line;
    int section = -1;  // 0-3 actuator, 4-7 blend
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            std::string label = line.substr(1, line.size() - 2);
            if (label.rfind("actuator", 0) == 0) {
                section = std::stoi(label.substr(8)) - 1;
            } else if (label.rfind("blend", 0) == 0) {
                section = 3 + std::stoi(label.substr(5));
            } else {
                section = -1;
            }
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos || section < 0) continue;
        std::string key = trim(line.substr(0, eq));
        double value = std::stod(trim(line.substr(eq + 1)));
        if (section >= 0 && section < 4) {
            apply_value(profile.actuators[section], key, value);
        } else if (section >= 4 && section < 8) {
            apply_value(profile.blends[section - 4], key, value);
        }
    }
    if (loaded) *loaded = true;
    return profile;
}

}  // namespace dynamic_hub
