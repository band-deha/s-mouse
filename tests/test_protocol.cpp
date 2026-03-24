#include "protocol.h"
#include "keymap.h"
#include <gtest/gtest.h>

using namespace smouse;

class ProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        reset_sequence();
    }
};

// ---------- Header serialization ----------

TEST_F(ProtocolTest, HeaderRoundTrip) {
    MessageHeader h;
    h.type = MessageType::MOUSE_MOVE;
    h.payload_length = 4;
    h.sequence = 42;

    uint8_t buf[HEADER_SIZE];
    serialize_header(h, buf);

    auto result = deserialize_header(buf, HEADER_SIZE);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, MessageType::MOUSE_MOVE);
    EXPECT_EQ(result->payload_length, 4);
    EXPECT_EQ(result->sequence, 42);
}

TEST_F(ProtocolTest, HeaderBigEndian) {
    MessageHeader h;
    h.type = MessageType::KEEPALIVE;  // 0x00FF
    h.payload_length = 0x0102;
    h.sequence = 0x01020304;

    uint8_t buf[HEADER_SIZE];
    serialize_header(h, buf);

    // Verify big-endian encoding
    EXPECT_EQ(buf[0], 0x00);  // type high byte
    EXPECT_EQ(buf[1], 0xFF);  // type low byte
    EXPECT_EQ(buf[2], 0x01);  // payload_length high
    EXPECT_EQ(buf[3], 0x02);  // payload_length low
    EXPECT_EQ(buf[4], 0x01);  // sequence byte 0
    EXPECT_EQ(buf[5], 0x02);  // sequence byte 1
    EXPECT_EQ(buf[6], 0x03);  // sequence byte 2
    EXPECT_EQ(buf[7], 0x04);  // sequence byte 3
}

TEST_F(ProtocolTest, HeaderTooShort) {
    uint8_t buf[4] = {0};
    auto result = deserialize_header(buf, 4);
    EXPECT_FALSE(result.has_value());
}

// ---------- MouseMove ----------

TEST_F(ProtocolTest, MouseMoveRoundTrip) {
    auto msg = make_message(MouseMove{-10, 20});
    EXPECT_EQ(msg.header.type, MessageType::MOUSE_MOVE);
    EXPECT_EQ(msg.header.sequence, 0);

    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& [decoded, consumed] = *result;
    EXPECT_EQ(consumed, data.size());
    EXPECT_EQ(decoded.header.type, MessageType::MOUSE_MOVE);

    auto& mm = std::get<MouseMove>(decoded.payload);
    EXPECT_EQ(mm.dx, -10);
    EXPECT_EQ(mm.dy, 20);
}

TEST_F(ProtocolTest, MouseMoveNegativeValues) {
    auto msg = make_message(MouseMove{-32768, 32767});
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& mm = std::get<MouseMove>(result->first.payload);
    EXPECT_EQ(mm.dx, -32768);
    EXPECT_EQ(mm.dy, 32767);
}

// ---------- MouseMoveAbs ----------

TEST_F(ProtocolTest, MouseMoveAbsRoundTrip) {
    auto msg = make_message(MouseMoveAbs{1920, 1080});
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& ma = std::get<MouseMoveAbs>(result->first.payload);
    EXPECT_EQ(ma.x, 1920);
    EXPECT_EQ(ma.y, 1080);
}

// ---------- MouseButton ----------

TEST_F(ProtocolTest, MouseButtonRoundTrip) {
    auto msg = make_message(MouseButton{1, 1});  // right button, pressed
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& mb = std::get<MouseButton>(result->first.payload);
    EXPECT_EQ(mb.button, 1);
    EXPECT_EQ(mb.pressed, 1);
}

// ---------- MouseScroll ----------

TEST_F(ProtocolTest, MouseScrollRoundTrip) {
    auto msg = make_message(MouseScroll{0, -3});
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& ms = std::get<MouseScroll>(result->first.payload);
    EXPECT_EQ(ms.dx, 0);
    EXPECT_EQ(ms.dy, -3);
}

// ---------- KeyDown / KeyUp ----------

TEST_F(ProtocolTest, KeyDownRoundTrip) {
    auto msg = make_message(KeyDown{0x04, MOD_SHIFT | MOD_CTRL});  // Key A + Shift + Ctrl
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& kd = std::get<KeyDown>(result->first.payload);
    EXPECT_EQ(kd.scancode, 0x04);
    EXPECT_EQ(kd.modifiers, MOD_SHIFT | MOD_CTRL);
}

TEST_F(ProtocolTest, KeyUpRoundTrip) {
    auto msg = make_message(KeyUp{0x28, 0});  // Enter, no modifiers
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& ku = std::get<KeyUp>(result->first.payload);
    EXPECT_EQ(ku.scancode, 0x28);
    EXPECT_EQ(ku.modifiers, 0u);
}

// ---------- ScreenEnter / ScreenLeave ----------

TEST_F(ProtocolTest, ScreenEnterRoundTrip) {
    auto msg = make_message(ScreenEnter{100, 200, 1920, 1080});
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& se = std::get<ScreenEnter>(result->first.payload);
    EXPECT_EQ(se.x, 100);
    EXPECT_EQ(se.y, 200);
    EXPECT_EQ(se.w, 1920);
    EXPECT_EQ(se.h, 1080);
}

TEST_F(ProtocolTest, ScreenLeaveRoundTrip) {
    auto msg = make_message(ScreenLeave{});
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<ScreenLeave>(result->first.payload));
}

// ---------- ClipboardUpdate ----------

TEST_F(ProtocolTest, ClipboardTextRoundTrip) {
    std::string text = "Hello, World! 🌍";
    ClipboardUpdate cb;
    cb.format = ClipboardFormat::TEXT;
    cb.data.assign(text.begin(), text.end());

    auto msg = make_message(cb);
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& decoded = std::get<ClipboardUpdate>(result->first.payload);
    EXPECT_EQ(decoded.format, ClipboardFormat::TEXT);
    std::string decoded_text(decoded.data.begin(), decoded.data.end());
    EXPECT_EQ(decoded_text, text);
}

TEST_F(ProtocolTest, ClipboardEmptyData) {
    ClipboardUpdate cb;
    cb.format = ClipboardFormat::TEXT;

    auto msg = make_message(cb);
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& decoded = std::get<ClipboardUpdate>(result->first.payload);
    EXPECT_TRUE(decoded.data.empty());
}

// ---------- Hello / HelloAck ----------

TEST_F(ProtocolTest, HelloRoundTrip) {
    Hello h;
    h.version = PROTOCOL_VERSION;
    h.screen_w = 2560;
    h.screen_h = 1440;
    h.name = "MacBook-Pro";

    auto msg = make_message(h);
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& decoded = std::get<Hello>(result->first.payload);
    EXPECT_EQ(decoded.version, PROTOCOL_VERSION);
    EXPECT_EQ(decoded.screen_w, 2560);
    EXPECT_EQ(decoded.screen_h, 1440);
    EXPECT_EQ(decoded.name, "MacBook-Pro");
}

TEST_F(ProtocolTest, HelloAckRoundTrip) {
    auto msg = make_message(HelloAck{PROTOCOL_VERSION, 1});
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    auto& decoded = std::get<HelloAck>(result->first.payload);
    EXPECT_EQ(decoded.version, PROTOCOL_VERSION);
    EXPECT_EQ(decoded.accepted, 1);
}

// ---------- Control messages ----------

TEST_F(ProtocolTest, KeepaliveRoundTrip) {
    auto msg = make_message(Keepalive{});
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<Keepalive>(result->first.payload));
    EXPECT_EQ(result->first.header.payload_length, 0);
}

TEST_F(ProtocolTest, DisconnectRoundTrip) {
    auto msg = make_message(Disconnect{});
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<Disconnect>(result->first.payload));
}

TEST_F(ProtocolTest, ReconnectRoundTrip) {
    auto msg = make_message(Reconnect{});
    auto data = serialize_message(msg);
    auto result = deserialize_message(data.data(), data.size());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<Reconnect>(result->first.payload));
}

// ---------- Sequence numbers ----------

TEST_F(ProtocolTest, SequenceAutoIncrement) {
    auto msg1 = make_message(Keepalive{});
    auto msg2 = make_message(Keepalive{});
    auto msg3 = make_message(Keepalive{});

    EXPECT_EQ(msg1.header.sequence, 0);
    EXPECT_EQ(msg2.header.sequence, 1);
    EXPECT_EQ(msg3.header.sequence, 2);
}

// ---------- Edge cases ----------

TEST_F(ProtocolTest, DeserializeIncompleteData) {
    auto msg = make_message(MouseMove{1, 2});
    auto data = serialize_message(msg);

    // Truncate data
    auto result = deserialize_message(data.data(), data.size() - 1);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ProtocolTest, MultipleMessagesInBuffer) {
    auto msg1 = make_message(MouseMove{1, 2});
    auto msg2 = make_message(KeyDown{0x04, 0});

    auto data1 = serialize_message(msg1);
    auto data2 = serialize_message(msg2);

    // Concatenate both messages
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), data1.begin(), data1.end());
    combined.insert(combined.end(), data2.begin(), data2.end());

    // Parse first message
    auto result1 = deserialize_message(combined.data(), combined.size());
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->second, data1.size());

    // Parse second message
    auto result2 = deserialize_message(
        combined.data() + result1->second, combined.size() - result1->second);
    ASSERT_TRUE(result2.has_value());
    auto& kd = std::get<KeyDown>(result2->first.payload);
    EXPECT_EQ(kd.scancode, 0x04);
}
