#pragma once

#include "actuator_profile.hpp"
#include "telemetry_frame.hpp"

namespace dynamic_hub {

class TelemetryRouter {
  public:
    explicit TelemetryRouter(const RigProfile& profile) : profile_(profile) {}

    ActuatorTargets map(const TelemetryFrame& frame) const;

  private:
    RigProfile profile_;
};

}  // namespace dynamic_hub
