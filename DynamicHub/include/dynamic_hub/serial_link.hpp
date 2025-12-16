#pragma once

#include "telemetry_frame.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace dynamic_hub {

class SerialLink {
  public:
    SerialLink() = default;
    SerialLink(const SerialLink&) = delete;
    SerialLink& operator=(const SerialLink&) = delete;

    bool open(const std::string& port, int baud);
    void close();

    bool is_open() const { return fd_ >= 0; }

    bool send_targets(uint64_t tick, const ActuatorTargets& targets);
    bool send_command(const std::string& command);

  private:
    int fd_{-1};
};

}  // namespace dynamic_hub
