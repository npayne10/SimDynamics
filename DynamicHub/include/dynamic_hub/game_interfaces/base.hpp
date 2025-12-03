#pragma once

#include "dynamic_hub/telemetry_frame.hpp"

#include <chrono>
#include <memory>
#include <string>

namespace dynamic_hub::game_interfaces {

class GameInterface {
  public:
    virtual ~GameInterface() = default;
    virtual std::string name() const = 0;
    virtual bool poll(TelemetryFrame& frame) = 0;
};

std::unique_ptr<GameInterface> make_interface(const std::string& id);

}  // namespace dynamic_hub::game_interfaces
