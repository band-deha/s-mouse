#include "udp_channel.h"

#include <cstring>

namespace smouse {

UdpChannel::UdpChannel() { ensure_winsock(); }

UdpChannel::UdpChannel(UdpChannel&& other) noexcept
    : fd_(other.fd_)
    , running_(other.running_)
    , remote_addr_(other.remote_addr_)
    , has_remote_(other.has_remote_) {
    recv_thread_ = std::move(other.recv_thread_);
    other.fd_ = SMOUSE_INVALID_SOCKET;
    other.running_ = false;
    other.has_remote_ = false;
}

UdpChannel& UdpChannel::operator=(UdpChannel&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        running_ = other.running_;
        remote_addr_ = other.remote_addr_;
        has_remote_ = other.has_remote_;
        recv_thread_ = std::move(other.recv_thread_);
        other.fd_ = SMOUSE_INVALID_SOCKET;
        other.running_ = false;
        other.has_remote_ = false;
    }
    return *this;
}

UdpChannel::~UdpChannel() {
    close();
}

bool UdpChannel::bind(uint16_t port) {
    ensure_winsock();

    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ == SMOUSE_INVALID_SOCKET) return false;

    // Allow address reuse
    int opt = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        net_close(fd_);
        fd_ = SMOUSE_INVALID_SOCKET;
        return false;
    }

    return true;
}

void UdpChannel::set_remote(const std::string& host, uint16_t port) {
    std::memset(&remote_addr_, 0, sizeof(remote_addr_));
    remote_addr_.sin_family = AF_INET;
    remote_addr_.sin_port = htons(port);

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0 && res) {
        auto* sa = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
        remote_addr_.sin_addr = sa->sin_addr;
        freeaddrinfo(res);
        has_remote_ = true;
    }
}

bool UdpChannel::send(const Message& msg) {
    if (fd_ == SMOUSE_INVALID_SOCKET || !has_remote_) return false;

    auto data = serialize_message(msg);

    std::lock_guard lock(send_mutex_);
    int n = ::sendto(fd_, reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()), 0,
        reinterpret_cast<const struct sockaddr*>(&remote_addr_),
        sizeof(remote_addr_));

    return n == static_cast<int>(data.size());
}

void UdpChannel::start_receive(MessageCallback on_message) {
    running_ = true;
    recv_thread_ = std::thread(&UdpChannel::recv_loop, this, std::move(on_message));
}

void UdpChannel::close() {
    running_ = false;
    if (fd_ != SMOUSE_INVALID_SOCKET) {
        net_shutdown(fd_);
        net_close(fd_);
        fd_ = SMOUSE_INVALID_SOCKET;
    }
    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
}

void UdpChannel::recv_loop(MessageCallback on_message) {
    // Max UDP datagram: header(8) + largest single-datagram payload
    // Mouse events are tiny (< 20 bytes total)
    char buf[512];

    while (running_) {
        int n = ::recvfrom(fd_, buf, sizeof(buf), 0, nullptr, nullptr);
        if (n <= 0) {
            if (!running_) break;
            continue;
        }

        auto msg_opt = deserialize_message(
            reinterpret_cast<uint8_t*>(buf), static_cast<size_t>(n));
        if (!msg_opt) continue;

        if (on_message) on_message(std::move(msg_opt->first));
    }
}

} // namespace smouse
