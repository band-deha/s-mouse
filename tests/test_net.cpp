#include "protocol.h"
#include "tcp_connection.h"
#include "udp_channel.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using namespace smouse;

class NetTest : public ::testing::Test {
protected:
    void SetUp() override {
        reset_sequence();
    }
};

// ---------- TCP Tests ----------

TEST_F(NetTest, TcpServerClientConnect) {
    std::atomic<bool> client_connected{false};
    std::mutex mtx;
    std::condition_variable cv;

    TcpServer server;
    TcpConnection server_conn;

    uint16_t port = 24810;
    ASSERT_TRUE(server.listen(port, [&](TcpConnection conn, std::string /*peer*/) {
        server_conn = std::move(conn);
        {
            std::lock_guard lock(mtx);
            client_connected = true;
        }
        cv.notify_one();
    }));

    TcpConnection client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));

    // Wait for server to accept
    {
        std::unique_lock lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(2), [&] { return client_connected.load(); });
    }
    EXPECT_TRUE(client_connected);

    client.close();
    server_conn.close();
    server.stop();
}

TEST_F(NetTest, TcpSendReceiveMessage) {
    std::mutex mtx;
    std::condition_variable cv;
    std::optional<Message> received_msg;
    TcpConnection server_conn;
    std::atomic<bool> accepted{false};

    uint16_t port = 24811;
    TcpServer server;
    ASSERT_TRUE(server.listen(port, [&](TcpConnection conn, std::string /*peer*/) {
        server_conn = std::move(conn);
        server_conn.start_receive(
            [&](Message msg) {
                std::lock_guard lock(mtx);
                received_msg = std::move(msg);
                cv.notify_one();
            },
            [&]() {}
        );
        accepted = true;
    }));

    TcpConnection client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));

    // Wait for server accept
    for (int i = 0; i < 20 && !accepted; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_TRUE(accepted);

    // Send a message
    auto msg = make_message(MouseMove{42, -17});
    ASSERT_TRUE(client.send(msg));

    // Wait for reception
    {
        std::unique_lock lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(2), [&] { return received_msg.has_value(); });
    }

    ASSERT_TRUE(received_msg.has_value());
    EXPECT_EQ(received_msg->header.type, MessageType::MOUSE_MOVE);
    auto& mm = std::get<MouseMove>(received_msg->payload);
    EXPECT_EQ(mm.dx, 42);
    EXPECT_EQ(mm.dy, -17);

    client.close();
    server_conn.close();
    server.stop();
}

TEST_F(NetTest, TcpMultipleMessages) {
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<Message> received;
    TcpConnection server_conn;
    std::atomic<bool> accepted{false};

    uint16_t port = 24812;
    TcpServer server;
    ASSERT_TRUE(server.listen(port, [&](TcpConnection conn, std::string /*peer*/) {
        server_conn = std::move(conn);
        server_conn.start_receive(
            [&](Message msg) {
                std::lock_guard lock(mtx);
                received.push_back(std::move(msg));
                cv.notify_one();
            },
            [&]() {}
        );
        accepted = true;
    }));

    TcpConnection client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));

    for (int i = 0; i < 20 && !accepted; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_TRUE(accepted);

    // Send multiple messages
    constexpr int NUM_MSGS = 100;
    for (int i = 0; i < NUM_MSGS; i++) {
        auto msg = make_message(MouseMove{static_cast<int16_t>(i), static_cast<int16_t>(-i)});
        ASSERT_TRUE(client.send(msg));
    }

    // Wait for all messages
    {
        std::unique_lock lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(5),
            [&] { return received.size() >= NUM_MSGS; });
    }

    EXPECT_EQ(received.size(), NUM_MSGS);

    // Verify order
    for (int i = 0; i < NUM_MSGS; i++) {
        auto& mm = std::get<MouseMove>(received[i].payload);
        EXPECT_EQ(mm.dx, static_cast<int16_t>(i));
        EXPECT_EQ(mm.dy, static_cast<int16_t>(-i));
    }

    client.close();
    server_conn.close();
    server.stop();
}

TEST_F(NetTest, TcpDisconnectDetection) {
    std::atomic<bool> disconnected{false};
    std::mutex mtx;
    std::condition_variable cv;
    TcpConnection server_conn;
    std::atomic<bool> accepted{false};

    uint16_t port = 24813;
    TcpServer server;
    ASSERT_TRUE(server.listen(port, [&](TcpConnection conn, std::string /*peer*/) {
        server_conn = std::move(conn);
        server_conn.start_receive(
            [](Message) {},
            [&]() {
                disconnected = true;
                cv.notify_one();
            }
        );
        accepted = true;
    }));

    TcpConnection client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));

    for (int i = 0; i < 20 && !accepted; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Close client -> server should detect disconnect
    client.close();

    {
        std::unique_lock lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(2), [&] { return disconnected.load(); });
    }
    EXPECT_TRUE(disconnected);

    server_conn.close();
    server.stop();
}

// ---------- UDP Tests ----------

TEST_F(NetTest, UdpSendReceive) {
    std::mutex mtx;
    std::condition_variable cv;
    std::optional<Message> received_msg;

    uint16_t port = 24821;

    UdpChannel receiver;
    ASSERT_TRUE(receiver.bind(port));
    receiver.start_receive([&](Message msg) {
        std::lock_guard lock(mtx);
        received_msg = std::move(msg);
        cv.notify_one();
    });

    UdpChannel sender;
    ASSERT_TRUE(sender.bind(0));  // Ephemeral port for sending
    sender.set_remote("127.0.0.1", port);

    auto msg = make_message(MouseMove{100, -50});
    ASSERT_TRUE(sender.send(msg));

    {
        std::unique_lock lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(2), [&] { return received_msg.has_value(); });
    }

    ASSERT_TRUE(received_msg.has_value());
    auto& mm = std::get<MouseMove>(received_msg->payload);
    EXPECT_EQ(mm.dx, 100);
    EXPECT_EQ(mm.dy, -50);

    sender.close();
    receiver.close();
}

TEST_F(NetTest, UdpMultipleMessages) {
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<Message> received;

    uint16_t port = 24822;

    UdpChannel receiver;
    ASSERT_TRUE(receiver.bind(port));
    receiver.start_receive([&](Message msg) {
        std::lock_guard lock(mtx);
        received.push_back(std::move(msg));
        cv.notify_one();
    });

    UdpChannel sender;
    ASSERT_TRUE(sender.bind(0));
    sender.set_remote("127.0.0.1", port);

    constexpr int NUM_MSGS = 50;
    for (int i = 0; i < NUM_MSGS; i++) {
        auto msg = make_message(MouseMove{static_cast<int16_t>(i), 0});
        sender.send(msg);
    }

    // UDP may lose some packets, but on localhost it should be reliable
    {
        std::unique_lock lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(3),
            [&] { return received.size() >= NUM_MSGS; });
    }

    // On localhost, expect all messages
    EXPECT_GE(received.size(), static_cast<size_t>(NUM_MSGS - 5));  // Allow some loss

    sender.close();
    receiver.close();
}

// ---------- Latency Test ----------

TEST_F(NetTest, UdpLatencyBenchmark) {
    uint16_t port = 24823;

    UdpChannel receiver;
    ASSERT_TRUE(receiver.bind(port));

    std::mutex mtx;
    std::condition_variable cv;
    std::chrono::steady_clock::time_point recv_time;
    std::atomic<bool> got_msg{false};

    receiver.start_receive([&](Message /*msg*/) {
        recv_time = std::chrono::steady_clock::now();
        got_msg = true;
        cv.notify_one();
    });

    UdpChannel sender;
    ASSERT_TRUE(sender.bind(0));
    sender.set_remote("127.0.0.1", port);

    auto send_time = std::chrono::steady_clock::now();
    auto msg = make_message(MouseMove{1, 1});
    sender.send(msg);

    {
        std::unique_lock lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(1), [&] { return got_msg.load(); });
    }

    ASSERT_TRUE(got_msg);

    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
        recv_time - send_time);

    // On localhost, latency should be well under 1ms
    EXPECT_LT(latency.count(), 5000);  // < 5ms

    // Print for info
    std::cout << "UDP localhost latency: " << latency.count() << " µs" << std::endl;

    sender.close();
    receiver.close();
}
