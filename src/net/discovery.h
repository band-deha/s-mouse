#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

namespace smouse {

struct DiscoveredServer {
    std::string name;
    std::string host;
    uint16_t port;
};

// LAN discovery using UDP broadcast
class Discovery {
public:
    using DiscoveryCallback = std::function<void(DiscoveredServer server)>;

    Discovery();
    ~Discovery();

    // Start broadcasting this server's presence
    bool start_broadcasting(const std::string& name, uint16_t port);

    // Start listening for server broadcasts
    bool start_listening(DiscoveryCallback callback);

    void stop();

    // Get list of discovered servers
    std::vector<DiscoveredServer> get_servers() const;

private:
    void broadcast_loop(std::string name, uint16_t port);
    void listen_loop(DiscoveryCallback callback);

    std::thread thread_;
    std::atomic<bool> running_{false};
    int fd_ = -1;

    mutable std::mutex servers_mutex_;
    std::vector<DiscoveredServer> servers_;

    static constexpr uint16_t DISCOVERY_PORT = 24802;
    static constexpr uint32_t MAGIC = 0x534D4F55; // "SMOU"
};

} // namespace smouse
