#pragma once

#include "protocol.h"
#include "clipboard.h"
#include "../net/tcp_connection.h"
#include "../net/udp_channel.h"
#include "../platform/platform.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace smouse {

enum class ClientState {
    DISCONNECTED,
    CONNECTED,        // Connected but not receiving input
    ACTIVE,           // Receiving and injecting input
    RECONNECTING,     // Trying to reconnect
};

class Client {
public:
    Client();
    ~Client();

    // Connect to a server
    bool connect(const std::string& host, uint16_t tcp_port = DEFAULT_TCP_PORT,
                 uint16_t udp_port = DEFAULT_UDP_PORT);

    // Disconnect
    void disconnect();

    // Set this machine's name
    void set_name(const std::string& name) { name_ = name; }

    // Get state
    ClientState state() const { return state_; }

    // Status callback
    using StateCallback = std::function<void(ClientState state)>;
    void set_state_callback(StateCallback cb) { state_callback_ = std::move(cb); }

    // Enable auto-reconnect
    void set_auto_reconnect(bool enable) { auto_reconnect_ = enable; }

    // Log callback for GUI
    using LogCallback = std::function<void(const std::string& message)>;
    void set_log_callback(LogCallback cb) { log_callback_ = std::move(cb); }

    bool is_connected() const {
        return state_ == ClientState::CONNECTED || state_ == ClientState::ACTIVE;
    }

private:
    void on_server_message(Message msg);
    void on_server_disconnected();
    void on_udp_message(Message msg);
    void inject_event(const Message& msg);
    void reconnect_loop();
    void keepalive_loop();

    TcpConnection tcp_;
    UdpChannel udp_;
    std::unique_ptr<InputInject> input_inject_;
    std::unique_ptr<ScreenQuery> screen_query_;
    std::unique_ptr<ClipboardMonitor> clipboard_monitor_;

    std::string name_ = "s-mouse-client";
    std::string server_host_;
    uint16_t server_tcp_port_ = DEFAULT_TCP_PORT;
    uint16_t server_udp_port_ = DEFAULT_UDP_PORT;

    std::atomic<ClientState> state_{ClientState::DISCONNECTED};
    std::atomic<bool> running_{false};
    std::atomic<bool> auto_reconnect_{true};

    std::thread reconnect_thread_;
    std::thread keepalive_thread_;

    StateCallback state_callback_;
    LogCallback log_callback_;

    void log(const std::string& message);
};

} // namespace smouse
