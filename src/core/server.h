#pragma once

#include "protocol.h"
#include "screen_layout.h"
#include "clipboard.h"
#include "../net/tcp_connection.h"
#include "../net/udp_channel.h"
#include "../platform/platform.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace smouse {

enum class ServerState {
    LOCAL_ACTIVE,       // Input goes to local machine
    CLIENT_ACTIVE,      // Input forwarded to a client
    DISCONNECTED,       // Lost connection to active client
};

struct ClientConnection {
    std::string client_id;
    std::string name;
    TcpConnection tcp;
    std::string udp_host;
    uint16_t udp_port;
    ScreenInfo screen;
    std::chrono::steady_clock::time_point last_keepalive;
    bool connected = true;
};

class Server {
public:
    Server();
    ~Server();

    // Start the server
    bool start(uint16_t tcp_port = DEFAULT_TCP_PORT,
               uint16_t udp_port = DEFAULT_UDP_PORT);

    // Stop the server
    void stop();

    // Configure screen layout
    ScreenLayout& layout() { return layout_; }

    // Get current state
    ServerState state() const { return state_; }
    std::string active_client_id() const { return active_client_id_; }

    // Manual switch (hotkey)
    void switch_to_client(const std::string& client_id);
    void switch_to_local();

    // Status callback
    using StateCallback = std::function<void(ServerState state, const std::string& client_id)>;
    void set_state_callback(StateCallback cb) { state_callback_ = std::move(cb); }

    bool is_running() const { return running_; }

private:
    void on_client_connected(TcpConnection conn, std::string peer_addr);
    void on_client_message(const std::string& client_id, Message msg);
    void on_client_disconnected(const std::string& client_id);

    void on_input_event(Message msg);
    void keepalive_loop();

    void transition_to_client(const std::string& client_id);
    void transition_to_local();
    void send_to_active_client(const Message& msg);

    TcpServer tcp_server_;
    UdpChannel udp_channel_;
    std::unique_ptr<InputCapture> input_capture_;
    std::unique_ptr<ScreenQuery> screen_query_;
    std::unique_ptr<ClipboardMonitor> clipboard_monitor_;

    ScreenLayout layout_;
    std::atomic<ServerState> state_{ServerState::LOCAL_ACTIVE};
    std::string active_client_id_;

    std::mutex clients_mutex_;
    std::unordered_map<std::string, std::unique_ptr<ClientConnection>> clients_;

    std::thread keepalive_thread_;
    std::atomic<bool> running_{false};

    StateCallback state_callback_;
};

} // namespace smouse
