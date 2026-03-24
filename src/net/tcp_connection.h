#pragma once

#include "net_compat.h"
#include "protocol.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace smouse {

// TCP connection wrapping a socket with message framing.
// Currently plain TCP; TLS will be added in Phase 7.
class TcpConnection {
public:
    using MessageCallback = std::function<void(Message msg)>;
    using DisconnectCallback = std::function<void()>;

    TcpConnection();
    ~TcpConnection();

    // Non-copyable
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    // Move
    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    // Create from existing socket fd (used by server after accept)
    static TcpConnection from_fd(SMOUSE_SOCKET fd);

    // Connect to a server
    bool connect(const std::string& host, uint16_t port);

    // Send a message (thread-safe)
    bool send(const Message& msg);

    // Start async receive loop (calls callback on each message)
    void start_receive(MessageCallback on_message, DisconnectCallback on_disconnect);

    // Stop receive loop and close connection
    void close();

    bool is_connected() const;

    // Get peer address info
    std::string peer_address() const;

private:
    SMOUSE_SOCKET fd_ = SMOUSE_INVALID_SOCKET;
    bool connected_ = false;
    std::thread recv_thread_;
    std::mutex send_mutex_;
    bool running_ = false;

    void recv_loop(MessageCallback on_message, DisconnectCallback on_disconnect);
    bool send_raw(const uint8_t* data, size_t len);
    bool recv_exact(uint8_t* buf, size_t len);
    void close_fd();
};

// TCP server that listens for incoming connections
class TcpServer {
public:
    using AcceptCallback = std::function<void(TcpConnection conn, std::string peer_addr)>;

    TcpServer();
    ~TcpServer();

    // Start listening on a port
    bool listen(uint16_t port, AcceptCallback on_accept);

    // Stop listening
    void stop();

    bool is_listening() const;

private:
    SMOUSE_SOCKET listen_fd_ = SMOUSE_INVALID_SOCKET;
    std::thread accept_thread_;
    bool running_ = false;

    void accept_loop(AcceptCallback on_accept);
};

} // namespace smouse
