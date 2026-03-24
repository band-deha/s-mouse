#include "screen_layout.h"
#include <gtest/gtest.h>

using namespace smouse;

class ScreenLayoutTest : public ::testing::Test {
protected:
    ScreenLayout layout;

    void SetUp() override {
        // Server screen: 1920x1080 at origin
        layout.set_server_screen({0, 0, 1920, 1080});
    }
};

TEST_F(ScreenLayoutTest, NoClients) {
    auto result = layout.check_edge(0, 540);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ScreenLayoutTest, LeftEdgeHit) {
    ScreenInfo client;
    client.name = "MacBook";
    client.client_id = "client1";
    client.rect = {0, 0, 1440, 900};
    client.adjacent_edge = Edge::LEFT;
    layout.add_client(client);

    // Hit left edge (middle of screen)
    auto result = layout.check_edge(0, 540);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "client1");
}

TEST_F(ScreenLayoutTest, RightEdgeHit) {
    ScreenInfo client;
    client.name = "Desktop";
    client.client_id = "client2";
    client.rect = {0, 0, 2560, 1440};
    client.adjacent_edge = Edge::RIGHT;
    layout.add_client(client);

    auto result = layout.check_edge(1919, 540);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "client2");
}

TEST_F(ScreenLayoutTest, TopEdgeHit) {
    ScreenInfo client;
    client.name = "Monitor";
    client.client_id = "client3";
    client.rect = {0, 0, 1920, 1080};
    client.adjacent_edge = Edge::TOP;
    layout.add_client(client);

    auto result = layout.check_edge(960, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "client3");
}

TEST_F(ScreenLayoutTest, BottomEdgeHit) {
    ScreenInfo client;
    client.name = "Laptop";
    client.client_id = "client4";
    client.rect = {0, 0, 1366, 768};
    client.adjacent_edge = Edge::BOTTOM;
    layout.add_client(client);

    auto result = layout.check_edge(960, 1079);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "client4");
}

TEST_F(ScreenLayoutTest, CornerDeadZone) {
    ScreenInfo client;
    client.name = "MacBook";
    client.client_id = "client1";
    client.rect = {0, 0, 1440, 900};
    client.adjacent_edge = Edge::LEFT;
    layout.add_client(client);

    // Top-left corner should be dead zone
    auto result = layout.check_edge(0, 2);
    EXPECT_FALSE(result.has_value());

    // Bottom-left corner should be dead zone
    result = layout.check_edge(0, 1078);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ScreenLayoutTest, NotAtEdge) {
    ScreenInfo client;
    client.name = "MacBook";
    client.client_id = "client1";
    client.rect = {0, 0, 1440, 900};
    client.adjacent_edge = Edge::LEFT;
    layout.add_client(client);

    // Center of screen - not at edge
    auto result = layout.check_edge(960, 540);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ScreenLayoutTest, MapToClientRightEdge) {
    ScreenInfo client;
    client.name = "Desktop";
    client.client_id = "client2";
    client.rect = {0, 0, 2560, 1440};
    client.adjacent_edge = Edge::RIGHT;
    layout.add_client(client);

    // Map from server right edge to client left entry
    auto [x, y] = layout.map_to_client("client2", Edge::RIGHT, 0.5f);
    EXPECT_EQ(x, 0);  // Left side of client
    EXPECT_EQ(y, 720); // Middle vertically
}

TEST_F(ScreenLayoutTest, MapToClientLeftEdge) {
    ScreenInfo client;
    client.name = "MacBook";
    client.client_id = "client1";
    client.rect = {0, 0, 1440, 900};
    client.adjacent_edge = Edge::LEFT;
    layout.add_client(client);

    auto [x, y] = layout.map_to_client("client1", Edge::LEFT, 0.5f);
    EXPECT_EQ(x, 1439);  // Right side of client
    EXPECT_EQ(y, 450);    // Middle vertically
}

TEST_F(ScreenLayoutTest, RemoveClient) {
    ScreenInfo client;
    client.name = "MacBook";
    client.client_id = "client1";
    client.rect = {0, 0, 1440, 900};
    client.adjacent_edge = Edge::LEFT;
    layout.add_client(client);

    layout.remove_client("client1");
    auto result = layout.check_edge(0, 540);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ScreenLayoutTest, MultipleClients) {
    ScreenInfo left;
    left.name = "Left";
    left.client_id = "left";
    left.rect = {0, 0, 1440, 900};
    left.adjacent_edge = Edge::LEFT;

    ScreenInfo right;
    right.name = "Right";
    right.client_id = "right";
    right.rect = {0, 0, 2560, 1440};
    right.adjacent_edge = Edge::RIGHT;

    layout.add_client(left);
    layout.add_client(right);

    auto result_left = layout.check_edge(0, 540);
    ASSERT_TRUE(result_left.has_value());
    EXPECT_EQ(*result_left, "left");

    auto result_right = layout.check_edge(1919, 540);
    ASSERT_TRUE(result_right.has_value());
    EXPECT_EQ(*result_right, "right");
}

TEST_F(ScreenLayoutTest, ReplaceClient) {
    ScreenInfo client1;
    client1.name = "Old";
    client1.client_id = "client1";
    client1.rect = {0, 0, 1440, 900};
    client1.adjacent_edge = Edge::LEFT;
    layout.add_client(client1);

    ScreenInfo client1_new;
    client1_new.name = "New";
    client1_new.client_id = "client1";
    client1_new.rect = {0, 0, 2560, 1440};
    client1_new.adjacent_edge = Edge::RIGHT;
    layout.add_client(client1_new);

    EXPECT_EQ(layout.clients().size(), 1u);

    auto info = layout.get_client("client1");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->name, "New");
    EXPECT_EQ(info->adjacent_edge, Edge::RIGHT);
}
