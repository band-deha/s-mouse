#include "client.h"
#include <iostream>
#include <sstream>

namespace smouse {

Client::Client() = default;

Client::~Client() {
    disconnect();
}

void Client::log(const std::string& message) {
    std::cout << message << std::endl;
    if (log_callback_) log_callback_(message);
}

bool Client::connect(const std::string& host, uint16_t tcp_port, uint16_t udp_port) {
    server_host_ = host;
    server_tcp_port_ = tcp_port;
    server_udp_port_ = udp_port;

    // Create platform components
    input_inject_ = create_input_inject();
    screen_query_ = create_screen_query();
    clipboard_monitor_ = create_clipboard_monitor();

    if (!input_inject_ || !screen_query_) {
        log("[Client] Failed to create platform components");
        return false;
    }

    // Connect TCP
    log("[Client] Connecting to " + host + ":" + std::to_string(tcp_port) + "...");
    if (!tcp_.connect(host, tcp_port)) {
        log("[Client] TCP connection failed");
        return false;
    }

    // Set up UDP
    if (!udp_.bind(udp_port)) {
        // Try ephemeral port if default is busy
        if (!udp_.bind(0)) {
            log("[Client] UDP bind failed");
            tcp_.close();
            return false;
        }
    }
    udp_.set_remote(host, server_udp_port_);

    // Start receiving
    tcp_.start_receive(
        [this](Message msg) { on_server_message(std::move(msg)); },
        [this]() { on_server_disconnected(); }
    );

    udp_.start_receive([this](Message msg) { on_udp_message(std::move(msg)); });

    // Send HELLO
    auto displays = screen_query_->get_displays();
    uint16_t w = 1920, h = 1080;
    for (const auto& d : displays) {
        if (d.is_primary) {
            w = static_cast<uint16_t>(d.width);
            h = static_cast<uint16_t>(d.height);
            break;
        }
    }

    Hello hello;
    hello.version = PROTOCOL_VERSION;
    hello.screen_w = w;
    hello.screen_h = h;
    hello.name = name_;
    tcp_.send(make_message(std::move(hello)));

    running_ = true;
    state_ = ClientState::CONNECTED;

    // Start keepalive
    keepalive_thread_ = std::thread(&Client::keepalive_loop, this);

    if (state_callback_) state_callback_(ClientState::CONNECTED);

    {
        std::ostringstream oss;
        oss << "[Client] Connected to " << host << ":" << tcp_port
            << " (screen: " << w << "x" << h << ")";
        log(oss.str());
    }
    return true;
}

void Client::disconnect() {
    running_ = false;
    auto_reconnect_ = false;

    if (tcp_.is_connected()) {
        tcp_.send(make_message(Disconnect{}));
    }

    tcp_.close();
    udp_.close();

    if (reconnect_thread_.joinable()) reconnect_thread_.join();
    if (keepalive_thread_.joinable()) keepalive_thread_.join();

    state_ = ClientState::DISCONNECTED;
    if (state_callback_) state_callback_(ClientState::DISCONNECTED);
    log("[Client] Disconnected");
}

void Client::on_server_message(Message msg) {
    switch (msg.header.type) {
    case MessageType::HELLO_ACK: {
        auto& ack = std::get<HelloAck>(msg.payload);
        if (ack.accepted) {
            log("[Client] Server accepted connection");
        } else {
            log("[Client] Server rejected connection");
            disconnect();
        }
        break;
    }
    case MessageType::SCREEN_ENTER: {
        auto& enter = std::get<ScreenEnter>(msg.payload);
        state_ = ClientState::ACTIVE;
        if (state_callback_) state_callback_(ClientState::ACTIVE);

        // Move mouse to entry point
        if (input_inject_) {
            input_inject_->move_mouse(enter.x, enter.y);
        }

        {
            std::ostringstream oss;
            oss << "[Client] Screen entered at (" << enter.x << "," << enter.y << ")";
            log(oss.str());
        }
        break;
    }
    case MessageType::SCREEN_LEAVE: {
        state_ = ClientState::CONNECTED;
        if (state_callback_) state_callback_(ClientState::CONNECTED);
        log("[Client] Screen left");
        break;
    }
    case MessageType::CLIPBOARD_UPDATE: {
        auto& cb = std::get<ClipboardUpdate>(msg.payload);
        if (clipboard_monitor_) {
            clipboard_monitor_->set_content(cb.format, cb.data);
        }
        log("[Client] Clipboard received (" + std::to_string(cb.data.size()) + " bytes)");
        break;
    }
    case MessageType::KEEPALIVE:
        // Server is alive
        break;
    case MessageType::DISCONNECT:
        on_server_disconnected();
        break;
    default:
        // Input events over TCP (button clicks, keypresses)
        inject_event(msg);
        break;
    }
}

void Client::on_server_disconnected() {
    log("[Client] Server disconnected");

    state_ = ClientState::DISCONNECTED;
    if (state_callback_) state_callback_(ClientState::DISCONNECTED);

    tcp_.close();
    udp_.close();

    if (auto_reconnect_ && running_) {
        state_ = ClientState::RECONNECTING;
        if (state_callback_) state_callback_(ClientState::RECONNECTING);
        reconnect_thread_ = std::thread(&Client::reconnect_loop, this);
    }
}

void Client::on_udp_message(Message msg) {
    if (state_ != ClientState::ACTIVE) return;
    inject_event(msg);
}

void Client::inject_event(const Message& msg) {
    if (!input_inject_) return;

    switch (msg.header.type) {
    case MessageType::MOUSE_MOVE: {
        auto& mm = std::get<MouseMove>(msg.payload);
        input_inject_->move_mouse_relative(mm.dx, mm.dy);
        break;
    }
    case MessageType::MOUSE_MOVE_ABS: {
        auto& ma = std::get<MouseMoveAbs>(msg.payload);
        input_inject_->move_mouse(ma.x, ma.y);
        break;
    }
    case MessageType::MOUSE_BUTTON: {
        auto& mb = std::get<MouseButton>(msg.payload);
        input_inject_->mouse_button(mb.button, mb.pressed != 0);
        break;
    }
    case MessageType::MOUSE_SCROLL: {
        auto& ms = std::get<MouseScroll>(msg.payload);
        input_inject_->mouse_scroll(ms.dx, ms.dy);
        break;
    }
    case MessageType::KEY_DOWN: {
        auto& kd = std::get<KeyDown>(msg.payload);
        input_inject_->key_down(kd.scancode, kd.modifiers);
        break;
    }
    case MessageType::KEY_UP: {
        auto& ku = std::get<KeyUp>(msg.payload);
        input_inject_->key_up(ku.scancode, ku.modifiers);
        break;
    }
    default:
        break;
    }
}

void Client::reconnect_loop() {
    int backoff_ms = 1000;
    constexpr int max_backoff_ms = 30000;

    while (running_ && auto_reconnect_ && state_ == ClientState::RECONNECTING) {
        log("[Client] Attempting reconnect in " + std::to_string(backoff_ms) + "ms...");
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));

        if (!running_ || !auto_reconnect_) break;

        TcpConnection new_tcp;
        if (new_tcp.connect(server_host_, server_tcp_port_)) {
            tcp_ = std::move(new_tcp);

            // Re-setup UDP
            udp_.close();
            udp_ = UdpChannel();
            udp_.bind(0);
            udp_.set_remote(server_host_, server_udp_port_);

            tcp_.start_receive(
                [this](Message msg) { on_server_message(std::move(msg)); },
                [this]() { on_server_disconnected(); }
            );
            udp_.start_receive([this](Message msg) { on_udp_message(std::move(msg)); });

            // Re-send HELLO
            auto displays = screen_query_->get_displays();
            uint16_t w = 1920, h = 1080;
            for (const auto& d : displays) {
                if (d.is_primary) {
                    w = static_cast<uint16_t>(d.width);
                    h = static_cast<uint16_t>(d.height);
                    break;
                }
            }

            Hello hello;
            hello.version = PROTOCOL_VERSION;
            hello.screen_w = w;
            hello.screen_h = h;
            hello.name = name_;
            tcp_.send(make_message(std::move(hello)));

            state_ = ClientState::CONNECTED;
            if (state_callback_) state_callback_(ClientState::CONNECTED);
            log("[Client] Reconnected!");
            return;
        }

        // Exponential backoff
        backoff_ms = (std::min)(backoff_ms * 2, max_backoff_ms);
    }
}

void Client::keepalive_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (tcp_.is_connected()) {
            tcp_.send(make_message(Keepalive{}));
        }
    }
}

} // namespace smouse
