#pragma once

// Cross-platform socket compatibility layer

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")

using ssize_t = int;
using socklen_t = int;

inline int net_close(SOCKET fd) { return closesocket(fd); }
inline int net_shutdown(SOCKET fd) { return shutdown(fd, SD_BOTH); }

// Windows needs WSAStartup before any socket call
struct WinsockInit {
    WinsockInit() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    ~WinsockInit() { WSACleanup(); }
};
inline void ensure_winsock() {
    static WinsockInit init;
}

#  define SMOUSE_INVALID_SOCKET INVALID_SOCKET
#  define SMOUSE_SOCKET SOCKET
#  define MSG_NOSIGNAL 0

#else
#  include <arpa/inet.h>
#  include <cerrno>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <sys/socket.h>
#  include <unistd.h>

inline int net_close(int fd) { return ::close(fd); }
inline int net_shutdown(int fd) { return ::shutdown(fd, SHUT_RDWR); }
inline void ensure_winsock() {}

#  define SMOUSE_INVALID_SOCKET (-1)
#  define SMOUSE_SOCKET int

#endif
