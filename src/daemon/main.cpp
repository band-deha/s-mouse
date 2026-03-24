#include "core/server.h"
#include "core/client.h"

#include <csignal>
#include <iostream>
#include <string>
#include <atomic>

static std::atomic<bool> g_running{true};

static void signal_handler(int /*sig*/) {
    g_running = false;
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <mode> [options]\n"
              << "\n"
              << "Modes:\n"
              << "  server [port]           Start as server (default port: 24800)\n"
              << "  client <host> [port]    Connect to a server\n"
              << "\n"
              << "Options:\n"
              << "  --name <name>           Set machine name\n"
              << "  --edge <left|right|top|bottom>  Which edge of server this client is on\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::string mode = argv[1];
    std::string name = "s-mouse";
    std::string edge = "right";

    // Parse optional flags
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--name" && i + 1 < argc) {
            name = argv[++i];
        } else if (arg == "--edge" && i + 1 < argc) {
            edge = argv[++i];
        }
    }

    if (mode == "server") {
        uint16_t port = smouse::DEFAULT_TCP_PORT;
        if (argc > 2 && argv[2][0] != '-') {
            port = static_cast<uint16_t>(std::stoi(argv[2]));
        }

        smouse::Server server;
        server.set_state_callback([](smouse::ServerState state, const std::string& client_id) {
            switch (state) {
            case smouse::ServerState::LOCAL_ACTIVE:
                std::cout << "[State] Local active" << std::endl;
                break;
            case smouse::ServerState::CLIENT_ACTIVE:
                std::cout << "[State] Client active: " << client_id << std::endl;
                break;
            case smouse::ServerState::DISCONNECTED:
                std::cout << "[State] Disconnected" << std::endl;
                break;
            }
        });

        if (!server.start(port)) {
            std::cerr << "Failed to start server on port " << port << std::endl;
            std::cerr << "Make sure you have Accessibility permission enabled." << std::endl;
            return 1;
        }

        std::cout << "Server started on port " << port << std::endl;
        std::cout << "Waiting for clients... (Ctrl+C to quit)" << std::endl;

        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        server.stop();

    } else if (mode == "client") {
        if (argc < 3) {
            std::cerr << "Error: client mode requires a host address" << std::endl;
            print_usage(argv[0]);
            return 1;
        }

        std::string host = argv[2];
        uint16_t port = smouse::DEFAULT_TCP_PORT;
        if (argc > 3 && argv[3][0] != '-') {
            port = static_cast<uint16_t>(std::stoi(argv[3]));
        }

        smouse::Client client;
        client.set_name(name);
        client.set_auto_reconnect(true);
        client.set_state_callback([](smouse::ClientState state) {
            switch (state) {
            case smouse::ClientState::DISCONNECTED:
                std::cout << "[State] Disconnected" << std::endl;
                break;
            case smouse::ClientState::CONNECTED:
                std::cout << "[State] Connected (idle)" << std::endl;
                break;
            case smouse::ClientState::ACTIVE:
                std::cout << "[State] Active (receiving input)" << std::endl;
                break;
            case smouse::ClientState::RECONNECTING:
                std::cout << "[State] Reconnecting..." << std::endl;
                break;
            }
        });

        if (!client.connect(host, port)) {
            std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
            return 1;
        }

        std::cout << "Connected to " << host << ":" << port << std::endl;
        std::cout << "Waiting for input... (Ctrl+C to quit)" << std::endl;

        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        client.disconnect();

    } else {
        std::cerr << "Unknown mode: " << mode << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
