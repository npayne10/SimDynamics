#pragma once

#include "dynamic_hub/game_interfaces/base.hpp"

namespace dynamic_hub::game_interfaces {

class AssettoCorsaInterface : public GameInterface {
  public:
    bool poll(TelemetryFrame& frame) override;
    std::string name() const override { return "Assetto Corsa"; }
};

}  // namespace dynamic_hub::game_interfaces
