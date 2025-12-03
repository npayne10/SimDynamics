#include "dynamic_hub/game_interfaces/base.hpp"

#include "dynamic_hub/game_interfaces/assetto_corsa.hpp"
#include "dynamic_hub/game_interfaces/assetto_corsa_competizione.hpp"
#include "dynamic_hub/game_interfaces/rfactor2.hpp"

#include <algorithm>
#include <cctype>

namespace dynamic_hub::game_interfaces {

std::unique_ptr<GameInterface> make_interface(const std::string& id) {
    std::string lowered = id;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "ac" || lowered == "assetto_corsa") {
        return std::make_unique<AssettoCorsaInterface>();
    }
    if (lowered == "acc" || lowered == "assetto_corsa_competizione") {
        return std::make_unique<AssettoCorsaCompetizioneInterface>();
    }
    if (lowered == "rf2" || lowered == "rfactor2") {
        return std::make_unique<Rfactor2Interface>();
    }
    return nullptr;
}

}  // namespace dynamic_hub::game_interfaces
