#include "dynamic_hub/game_interfaces/assetto_corsa_competizione.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace dynamic_hub::game_interfaces {
namespace {
constexpr int kPort = 9000;

int ensure_socket() {
    static int sock = -1;
    if (sock >= 0) return sock;
    sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "ACC interface: unable to open UDP socket" << std::endl;
        return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(kPort);
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "ACC interface: bind failed" << std::endl;
        ::close(sock);
        sock = -1;
        return -1;
    }
    return sock;
}

bool parse_payload(const std::string& message, TelemetryFrame& frame) {
    std::stringstream ss(message);
    return static_cast<bool>(ss >> frame.surge_mps2 >> frame.sway_mps2 >> frame.heave_mps2 >> frame.roll_rad >> frame.pitch_rad >> frame.yaw_rad);
}
}

bool AssettoCorsaCompetizioneInterface::poll(TelemetryFrame& frame) {
    int sock = ensure_socket();
    if (sock < 0) return false;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    timeval tv{0, 50000};
    int ready = select(sock + 1, &fds, nullptr, nullptr, &tv);
    if (ready <= 0) return false;
    char buffer[512] = {0};
    auto len = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (len <= 0) return false;
    buffer[len] = '\0';
    std::string message(buffer);
    return parse_payload(message, frame);
}

}  // namespace dynamic_hub::game_interfaces
