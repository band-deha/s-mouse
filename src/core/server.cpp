#include "server.h"
#include <iostream>

namespace smouse {

Server::Server() = default;

Server::~Server() {
    stop();
}

bool Server::start(uint16_t tcp_port, uint16_t udp_port) {
    if (running_) return false;

    // Create platform components
    input_capture_ = create_input_capture();
    screen_query_ = create_screen_query();
    clipboard_monitor_ = create_clipboard_monitor();

    if (!input_capture_ || !screen_query_) {
        return false;
    }

    // Set up server screen from display info
    auto displays = screen_query_->get_displays();
    if (!displays.empty()) {
        for (const auto& d : displays) {
            if (d.is_primary) {
                layout_.set_server_screen({d.x, d.y, d.width, d.height});
                break;
            }
        }
    }

    // Start TCP server
    if (!tcp_server_.listen(tcp_port, [this](TcpConnection conn, std::string peer) {
        on_client_connected(std::move(conn), std::move(peer));
    })) {
        return false;
    }

    // Start UDP channel
    if (!udp_channel_.bind(udp_port)) {
        tcp_server_.stop();
        return false;
    }

    // Start input capture
    if (!input_capture_->start([this](Message msg) {
        on_input_event(std::move(msg));
    })) {
        tcp_server_.stop();
        udp_channel_.close();
        return false;
    }

    // Start clipboard monitoring
    if (clipboard_monitor_) {
        clipboard_monitor_->start([](ClipboardFormat /*fmt*/, std::vector<uint8_t> /*data*/) {
            // Clipboard changed locally - will be sent on next screen transition
        });
    }

    running_ = true;

    // Start keepalive thread
    keepalive_thread_ = std::thread(&Server::keepalive_loop, this);

    state_ = ServerState::LOCAL_ACTIVE;
    return true;
}

void Server::stop() {
    running_ = false;

    if (input_capture_) input_capture_->stop();
    if (clipboard_monitor_) clipboard_monitor_->stop();

    tcp_server_.stop();
    udp_channel_.close();

    if (keepalive_thread_.joinable()) {
        keepalive_thread_.join();
    }

    std::lock_guard lock(clients_mutex_);
    clients_.clear();
}

void Server::on_client_connected(TcpConnection conn, std::string peer_addr) {
    // Wait for HELLO message from client
    // For now, generate a client_id from the peer address
    std::string client_id = peer_addr;

    auto client = std::make_unique<ClientConnection>();
    client->client_id = client_id;
    client->tcp = std::move(conn);
    client->last_keepalive = std::chrono::steady_clock::now();

    // Extract host for UDP
    auto colon = peer_addr.find(':');
    if (colon != std::string::npos) {
        client->udp_host = peer_addr.substr(0, colon);
        client->udp_port = DEFAULT_UDP_PORT;
    }

    auto* client_ptr = client.get();

    client_ptr->tcp.start_receive(
        [this, cid = client_id](Message msg) {
            on_client_message(cid, std::move(msg));
        },
        [this, cid = client_id]() {
            on_client_disconnected(cid);
        }
    );

    {
        std::lock_guard lock(clients_mutex_);
        clients_[client_id] = std::move(client);
    }

    std::cout << "[Server] Client connected: " << peer_addr << std::endl;
}

void Server::on_client_message(const std::string& client_id, Message msg) {
    std::lock_guard lock(clients_mutex_);
    auto it = clients_.find(client_id);
    if (it == clients_.end()) return;

    auto& client = *it->second;
    client.last_keepalive = std::chrono::steady_clock::now();

    switch (msg.header.type) {
    case MessageType::HELLO: {
        auto& hello = std::get<Hello>(msg.payload);
        client.name = hello.name;
        client.screen.name = hello.name;
        client.screen.client_id = client_id;
        client.screen.rect = {0, 0, hello.screen_w, hello.screen_h};

        // Send ACK
        auto ack = make_message(HelloAck{PROTOCOL_VERSION, 1});
        client.tcp.send(ack);

        std::cout << "[Server] Client hello: " << hello.name
                  << " (" << hello.screen_w << "x" << hello.screen_h << ")" << std::endl;
        break;
    }
    case MessageType::KEEPALIVE:
        // Already updated last_keepalive above
        break;
    case MessageType::DISCONNECT:
        on_client_disconnected(client_id);
        break;
    default:
        break;
    }
}

void Server::on_client_disconnected(const std::string& client_id) {
    bool was_active = (state_ == ServerState::CLIENT_ACTIVE &&
                       active_client_id_ == client_id);

    {
        std::lock_guard lock(clients_mutex_);
        clients_.erase(client_id);
    }

    layout_.remove_client(client_id);

    if (was_active) {
        transition_to_local();
    }

    std::cout << "[Server] Client disconnected: " << client_id << std::endl;
}

void Server::on_input_event(Message msg) {
    if (state_ == ServerState::CLIENT_ACTIVE) {
        // Forward all events to the active client
        send_to_active_client(msg);
        return;
    }

    // In LOCAL_ACTIVE mode, check for screen edge transitions
    if (msg.header.type == MessageType::MOUSE_MOVE_ABS) {
        auto& abs = std::get<MouseMoveAbs>(msg.payload);
        auto client_id = layout_.check_edge(abs.x, abs.y);
        if (client_id) {
            transition_to_client(*client_id);
        }
    }
}

void Server::switch_to_client(const std::string& client_id) {
    transition_to_client(client_id);
}

void Server::switch_to_local() {
    transition_to_local();
}

void Server::transition_to_client(const std::string& client_id) {
    std::lock_guard lock(clients_mutex_);
    auto it = clients_.find(client_id);
    if (it == clients_.end()) return;

    auto& client = *it->second;

    // Send clipboard data to client
    if (clipboard_monitor_) {
        auto content = clipboard_monitor_->get_content();
        if (content) {
            ClipboardUpdate cb;
            cb.format = content->first;
            cb.data = std::move(content->second);
            client.tcp.send(make_message(std::move(cb)));
        }
    }

    // Send screen enter
    auto screen_info = layout_.get_client(client_id);
    uint16_t entry_x = 0, entry_y = 0;
    if (screen_info) {
        entry_x = screen_info->rect.width / 2;
        entry_y = screen_info->rect.height / 2;
    }

    auto enter_msg = make_message(ScreenEnter{
        entry_x, entry_y,
        screen_info ? static_cast<uint16_t>(screen_info->rect.width) : uint16_t(1920),
        screen_info ? static_cast<uint16_t>(screen_info->rect.height) : uint16_t(1080)
    });
    client.tcp.send(enter_msg);

    // Set up UDP target
    udp_channel_.set_remote(client.udp_host, client.udp_port);

    // Suppress local input
    if (input_capture_) {
        input_capture_->set_suppress(true);
    }

    active_client_id_ = client_id;
    state_ = ServerState::CLIENT_ACTIVE;

    if (state_callback_) {
        state_callback_(ServerState::CLIENT_ACTIVE, client_id);
    }

    std::cout << "[Server] Switched to client: " << client.name << std::endl;
}

void Server::transition_to_local() {
    // Send screen leave to the active client
    if (!active_client_id_.empty()) {
        std::lock_guard lock(clients_mutex_);
        auto it = clients_.find(active_client_id_);
        if (it != clients_.end()) {
            it->second->tcp.send(make_message(ScreenLeave{}));
        }
    }

    // Stop suppressing local input
    if (input_capture_) {
        input_capture_->set_suppress(false);
    }

    active_client_id_.clear();
    state_ = ServerState::LOCAL_ACTIVE;

    if (state_callback_) {
        state_callback_(ServerState::LOCAL_ACTIVE, "");
    }

    std::cout << "[Server] Switched to local" << std::endl;
}

void Server::send_to_active_client(const Message& msg) {
    // Use UDP for mouse move events, TCP for everything else
    bool use_udp = (msg.header.type == MessageType::MOUSE_MOVE ||
                    msg.header.type == MessageType::MOUSE_MOVE_ABS ||
                    msg.header.type == MessageType::MOUSE_SCROLL);

    if (use_udp) {
        udp_channel_.send(msg);
    } else {
        std::lock_guard lock(clients_mutex_);
        auto it = clients_.find(active_client_id_);
        if (it != clients_.end()) {
            it->second->tcp.send(msg);
        }
    }
}

void Server::keepalive_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto now = std::chrono::steady_clock::now();

        std::lock_guard lock(clients_mutex_);
        for (auto it = clients_.begin(); it != clients_.end();) {
            auto& client = *it->second;
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - client.last_keepalive);

            if (elapsed.count() > 3) {
                std::string cid = it->first;
                bool was_active = (state_ == ServerState::CLIENT_ACTIVE &&
                                   active_client_id_ == cid);
                it = clients_.erase(it);

                if (was_active) {
                    // Need to unlock before transition
                    // For simplicity, just update state directly here
                    if (input_capture_) input_capture_->set_suppress(false);
                    active_client_id_.clear();
                    state_ = ServerState::LOCAL_ACTIVE;
                    if (state_callback_) {
                        state_callback_(ServerState::LOCAL_ACTIVE, "");
                    }
                    std::cout << "[Server] Client timed out: " << cid << std::endl;
                }
            } else {
                // Send keepalive
                client.tcp.send(make_message(Keepalive{}));
                ++it;
            }
        }
    }
}

} // namespace smouse
