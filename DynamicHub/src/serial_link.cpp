#include "dynamic_hub/serial_link.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <termios.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace dynamic_hub {

namespace {
#ifdef _WIN32
// Placeholder for Windows serial support.
struct SerialHandle {
    HANDLE handle{INVALID_HANDLE_VALUE};
};
#else
struct SerialHandle {
    int fd{-1};
};
#endif

int baud_to_flag(int baud) {
#ifndef _WIN32
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return B115200;
    }
#else
    return baud;
#endif
}

}  // namespace

bool SerialLink::open(const std::string& port, int baud) {
#ifdef _WIN32
    (void)port;
    (void)baud;
    fd_ = -1;
    std::cerr << "SerialLink: Windows support not implemented; use WSL or Linux." << std::endl;
    return false;
#else
    fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        std::cerr << "Failed to open serial port " << port << ": " << std::strerror(errno) << std::endl;
        return false;
    }

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "Failed to read serial attributes: " << std::strerror(errno) << std::endl;
        close();
        return false;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, baud_to_flag(baud));
    cfsetospeed(&tty, baud_to_flag(baud));
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "Failed to apply serial attributes: " << std::strerror(errno) << std::endl;
        close();
        return false;
    }
    return true;
#endif
}

void SerialLink::close() {
#ifndef _WIN32
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#else
    fd_ = -1;
#endif
}

bool SerialLink::send_targets(uint64_t tick, const ActuatorTargets& targets) {
    if (fd_ < 0) return false;
    std::ostringstream ss;
    ss << tick;
    for (const auto target : targets) {
        ss << ',' << static_cast<int>(target);
    }
    ss << "\n";
    auto payload = ss.str();
#ifndef _WIN32
    ssize_t written = ::write(fd_, payload.data(), payload.size());
    return written == static_cast<ssize_t>(payload.size());
#else
    return false;
#endif
}

bool SerialLink::send_command(const std::string& command) {
    if (fd_ < 0) return false;
    std::string payload = command + "\n";
#ifndef _WIN32
    ssize_t written = ::write(fd_, payload.data(), payload.size());
    return written == static_cast<ssize_t>(payload.size());
#else
    return false;
#endif
}

}  // namespace dynamic_hub
