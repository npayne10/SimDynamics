#include "dynamic_hub/actuator_profile.hpp"
#include "dynamic_hub/config_parser.hpp"
#include "dynamic_hub/game_interfaces/base.hpp"
#include "dynamic_hub/serial_link.hpp"
#include "dynamic_hub/telemetry_router.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {
std::atomic<bool> g_running{true};

struct Options {
    std::string game_id{"acc"};
    std::string serial_port{"/dev/ttyUSB0"};
    int baud{115200};
    std::string profile_path{"config/rig_profile.ini"};
    bool home{false};
    bool selftest{false};
};

void usage() {
    std::cout << "DynamicHub - C++ motion router\n"
              << "Usage: dynamic_hub --game <ac|acc|rf2> --port <serial> [--baud 115200] [--profile path] [--home] [--selftest]\n";
}

Options parse_args(int argc, char** argv) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--game" && i + 1 < argc) {
            opts.game_id = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            opts.serial_port = argv[++i];
        } else if (arg == "--baud" && i + 1 < argc) {
            opts.baud = std::stoi(argv[++i]);
        } else if (arg == "--profile" && i + 1 < argc) {
            opts.profile_path = argv[++i];
        } else if (arg == "--home") {
            opts.home = true;
        } else if (arg == "--selftest") {
            opts.selftest = true;
        } else if (arg == "--help" || arg == "-h") {
            usage();
            std::exit(0);
        }
    }
    return opts;
}

void handle_signal(int) { g_running = false; }

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    Options opts = parse_args(argc, argv);
    bool loaded = false;
    dynamic_hub::RigProfile profile = dynamic_hub::ConfigParser::load_profile(opts.profile_path, &loaded);
    if (!loaded) {
        std::cout << "Profile not found at " << opts.profile_path << ", using defaults." << std::endl;
    }

    dynamic_hub::TelemetryRouter router(profile);
    dynamic_hub::SerialLink link;
    if (!link.open(opts.serial_port, opts.baud)) {
        std::cerr << "Failed to open serial port. Exiting." << std::endl;
        return 1;
    }

    if (opts.home) {
        std::cout << "Sending HOME command..." << std::endl;
        link.send_command("HOME");
    }
    if (opts.selftest) {
        std::cout << "Sending SELFTEST command..." << std::endl;
        link.send_command("SELFTEST");
    }

    auto iface = dynamic_hub::game_interfaces::make_interface(opts.game_id);
    if (!iface) {
        std::cerr << "Unknown game interface id: " << opts.game_id << std::endl;
        return 1;
    }

    std::cout << "Running DynamicHub for " << iface->name() << " on " << opts.serial_port << " @ " << opts.baud << " baud" << std::endl;
    uint64_t tick = 0;
    while (g_running.load()) {
        dynamic_hub::TelemetryFrame frame{};
        if (iface->poll(frame)) {
            dynamic_hub::ActuatorTargets targets = router.map(frame);
            link.send_targets(tick++, targets);
        } else {
            std::this_thread::sleep_for(5ms);
        }
    }

    std::cout << "Shutting down." << std::endl;
    link.close();
    return 0;
}
