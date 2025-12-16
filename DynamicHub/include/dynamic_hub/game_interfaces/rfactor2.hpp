#pragma once

#include "dynamic_hub/game_interfaces/base.hpp"

namespace dynamic_hub::game_interfaces {

class Rfactor2Interface : public GameInterface {
  public:
    bool poll(TelemetryFrame& frame) override;
    std::string name() const override { return "rFactor 2"; }
};

}  // namespace dynamic_hub::game_interfaces
