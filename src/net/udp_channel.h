#pragma once

#include "protocol.h"
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <netinet/in.h>

namespace smouse {

// UDP channel for low-latency mouse movement events.
// Each message is self-contained in a single UDP datagram.
class UdpChannel {
public:
    using MessageCallback = std::function<void(Message msg)>;

    UdpChannel();
    ~UdpChannel();

    // Non-copyable
    UdpChannel(const UdpChannel&) = delete;
    UdpChannel& operator=(const UdpChannel&) = delete;

    // Move
    UdpChannel(UdpChannel&& other) noexcept;
    UdpChannel& operator=(UdpChannel&& other) noexcept;

    // Bind to a local port for receiving
    bool bind(uint16_t port);

    // Set the remote endpoint for sending
    void set_remote(const std::string& host, uint16_t port);

    // Send a message (thread-safe)
    bool send(const Message& msg);

    // Start async receive loop
    void start_receive(MessageCallback on_message);

    // Stop receiving and close
    void close();

    bool is_open() const { return fd_ >= 0; }

private:
    int fd_ = -1;
    std::thread recv_thread_;
    bool running_ = false;
    std::mutex send_mutex_;

    // Remote address for sending
    struct sockaddr_in remote_addr_{};
    bool has_remote_ = false;

    void recv_loop(MessageCallback on_message);
};

} // namespace smouse
