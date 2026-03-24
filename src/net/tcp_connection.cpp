#include "tcp_connection.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace smouse {

// ---------- TcpConnection ----------

TcpConnection::TcpConnection() = default;

TcpConnection::~TcpConnection() {
    close();
}

TcpConnection::TcpConnection(TcpConnection&& other) noexcept
    : fd_(other.fd_)
    , connected_(other.connected_)
    , recv_thread_(std::move(other.recv_thread_))
    , running_(other.running_) {
    other.fd_ = -1;
    other.connected_ = false;
    other.running_ = false;
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        connected_ = other.connected_;
        recv_thread_ = std::move(other.recv_thread_);
        running_ = other.running_;
        other.fd_ = -1;
        other.connected_ = false;
        other.running_ = false;
    }
    return *this;
}

TcpConnection TcpConnection::from_fd(int fd) {
    TcpConnection conn;
    conn.fd_ = fd;
    conn.connected_ = true;

    // Set TCP_NODELAY for low latency
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    return conn;
}

bool TcpConnection::connect(const std::string& host, uint16_t port) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
        return false;
    }

    fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd_ < 0) {
        freeaddrinfo(res);
        return false;
    }

    if (::connect(fd_, res->ai_addr, res->ai_addrlen) < 0) {
        ::close(fd_);
        fd_ = -1;
        freeaddrinfo(res);
        return false;
    }

    freeaddrinfo(res);

    // Set TCP_NODELAY
    int flag = 1;
    setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    connected_ = true;
    return true;
}

bool TcpConnection::send(const Message& msg) {
    auto data = serialize_message(msg);
    std::lock_guard lock(send_mutex_);
    return send_raw(data.data(), data.size());
}

void TcpConnection::start_receive(MessageCallback on_message, DisconnectCallback on_disconnect) {
    running_ = true;
    recv_thread_ = std::thread(&TcpConnection::recv_loop, this,
                               std::move(on_message), std::move(on_disconnect));
}

void TcpConnection::close() {
    running_ = false;
    connected_ = false;
    close_fd();
    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
}

bool TcpConnection::is_connected() const {
    return connected_;
}

std::string TcpConnection::peer_address() const {
    if (fd_ < 0) return "";

    struct sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getpeername(fd_, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        return "";
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

void TcpConnection::recv_loop(MessageCallback on_message, DisconnectCallback on_disconnect) {
    uint8_t header_buf[HEADER_SIZE];

    while (running_ && connected_) {
        // Read header
        if (!recv_exact(header_buf, HEADER_SIZE)) {
            connected_ = false;
            if (on_disconnect) on_disconnect();
            return;
        }

        auto header_opt = deserialize_header(header_buf, HEADER_SIZE);
        if (!header_opt) {
            connected_ = false;
            if (on_disconnect) on_disconnect();
            return;
        }

        // Read payload
        std::vector<uint8_t> payload_buf(header_opt->payload_length);
        if (header_opt->payload_length > 0) {
            if (!recv_exact(payload_buf.data(), header_opt->payload_length)) {
                connected_ = false;
                if (on_disconnect) on_disconnect();
                return;
            }
        }

        // Deserialize payload
        auto payload_opt = deserialize_payload(
            header_opt->type, payload_buf.data(), payload_buf.size());
        if (!payload_opt) continue;  // Skip malformed payloads

        Message msg;
        msg.header = *header_opt;
        msg.payload = std::move(*payload_opt);

        if (on_message) on_message(std::move(msg));
    }
}

bool TcpConnection::send_raw(const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd_, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            connected_ = false;
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool TcpConnection::recv_exact(uint8_t* buf, size_t len) {
    size_t received = 0;
    while (received < len && running_) {
        ssize_t n = ::recv(fd_, buf + received, len - received, 0);
        if (n <= 0) return false;
        received += static_cast<size_t>(n);
    }
    return received == len;
}

void TcpConnection::close_fd() {
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        fd_ = -1;
    }
}

// ---------- TcpServer ----------

TcpServer::TcpServer() = default;

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::listen(uint16_t port, AcceptCallback on_accept) {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) return false;

    // Allow address reuse
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (::listen(listen_fd_, 5) < 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    running_ = true;
    accept_thread_ = std::thread(&TcpServer::accept_loop, this, std::move(on_accept));
    return true;
}

void TcpServer::stop() {
    running_ = false;
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
}

bool TcpServer::is_listening() const {
    return running_;
}

void TcpServer::accept_loop(AcceptCallback on_accept) {
    while (running_) {
        struct sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = ::accept(listen_fd_,
            reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);

        if (client_fd < 0) {
            if (!running_) break;
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        std::string peer = std::string(ip) + ":" + std::to_string(ntohs(client_addr.sin_port));

        auto conn = TcpConnection::from_fd(client_fd);
        if (on_accept) on_accept(std::move(conn), std::move(peer));
    }
}

} // namespace smouse
