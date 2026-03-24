#include "discovery.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>

namespace smouse {

Discovery::Discovery() = default;

Discovery::~Discovery() {
    stop();
}

bool Discovery::start_broadcasting(const std::string& name, uint16_t port) {
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) return false;

    int opt = 1;
    setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    running_ = true;
    thread_ = std::thread(&Discovery::broadcast_loop, this, name, port);
    return true;
}

bool Discovery::start_listening(DiscoveryCallback callback) {
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) return false;

    int opt = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(DISCOVERY_PORT);

    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    running_ = true;
    thread_ = std::thread(&Discovery::listen_loop, this, std::move(callback));
    return true;
}

void Discovery::stop() {
    running_ = false;
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        fd_ = -1;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

std::vector<DiscoveredServer> Discovery::get_servers() const {
    std::lock_guard lock(servers_mutex_);
    return servers_;
}

void Discovery::broadcast_loop(std::string name, uint16_t port) {
    struct sockaddr_in broadcast_addr{};
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcast_addr.sin_port = htons(DISCOVERY_PORT);

    // Packet format: MAGIC(4) + PORT(2) + NAME_LEN(1) + NAME
    uint8_t packet[256];
    packet[0] = (MAGIC >> 24) & 0xFF;
    packet[1] = (MAGIC >> 16) & 0xFF;
    packet[2] = (MAGIC >> 8) & 0xFF;
    packet[3] = MAGIC & 0xFF;
    packet[4] = (port >> 8) & 0xFF;
    packet[5] = port & 0xFF;
    uint8_t name_len = static_cast<uint8_t>(std::min(name.size(), size_t{248}));
    packet[6] = name_len;
    std::memcpy(packet + 7, name.data(), name_len);
    size_t packet_size = 7 + name_len;

    while (running_) {
        ::sendto(fd_, packet, packet_size, 0,
            reinterpret_cast<struct sockaddr*>(&broadcast_addr),
            sizeof(broadcast_addr));

        // Broadcast every 2 seconds
        for (int i = 0; i < 20 && running_; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void Discovery::listen_loop(DiscoveryCallback callback) {
    uint8_t buf[256];

    while (running_) {
        struct sockaddr_in sender_addr{};
        socklen_t sender_len = sizeof(sender_addr);

        ssize_t n = ::recvfrom(fd_, buf, sizeof(buf), 0,
            reinterpret_cast<struct sockaddr*>(&sender_addr), &sender_len);

        if (n < 7) continue;

        // Verify magic
        uint32_t magic = (static_cast<uint32_t>(buf[0]) << 24) |
                         (static_cast<uint32_t>(buf[1]) << 16) |
                         (static_cast<uint32_t>(buf[2]) << 8) |
                         static_cast<uint32_t>(buf[3]);
        if (magic != MAGIC) continue;

        uint16_t port = (static_cast<uint16_t>(buf[4]) << 8) | buf[5];
        uint8_t name_len = buf[6];
        if (n < 7 + name_len) continue;

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender_addr.sin_addr, ip, sizeof(ip));

        DiscoveredServer server;
        server.host = ip;
        server.port = port;
        server.name.assign(reinterpret_cast<char*>(buf + 7), name_len);

        // Deduplicate
        {
            std::lock_guard lock(servers_mutex_);
            auto it = std::find_if(servers_.begin(), servers_.end(),
                [&](const DiscoveredServer& s) { return s.host == server.host; });
            if (it != servers_.end()) {
                *it = server;
            } else {
                servers_.push_back(server);
            }
        }

        if (callback) callback(std::move(server));
    }
}

} // namespace smouse
