#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace smouse {

// Wire protocol version
constexpr uint16_t PROTOCOL_VERSION = 1;

// Default ports
constexpr uint16_t DEFAULT_TCP_PORT = 24800;
constexpr uint16_t DEFAULT_UDP_PORT = 24801;

// Header size: 8 bytes
constexpr size_t HEADER_SIZE = 8;

// Max payload size: 10MB (for clipboard data)
constexpr size_t MAX_PAYLOAD_SIZE = 10 * 1024 * 1024;

// Message types
enum class MessageType : uint16_t {
    // Mouse events (0x00xx)
    MOUSE_MOVE       = 0x0001,  // Relative movement (dx, dy) - UDP
    MOUSE_MOVE_ABS   = 0x0002,  // Absolute position (x, y) - UDP
    MOUSE_BUTTON     = 0x0003,  // Button press/release - TCP
    MOUSE_SCROLL     = 0x0004,  // Scroll wheel (dx, dy) - UDP

    // Keyboard events (0x00xx)
    KEY_DOWN         = 0x0005,  // Key press - TCP
    KEY_UP           = 0x0006,  // Key release - TCP

    // Screen transition (0x00xx)
    SCREEN_ENTER     = 0x0007,  // Cursor entering client screen - TCP
    SCREEN_LEAVE     = 0x0008,  // Cursor leaving client screen - TCP

    // Clipboard (0x001x)
    CLIPBOARD_UPDATE = 0x0010,  // Clipboard data transfer - TCP

    // Connection management (0x002x)
    HELLO            = 0x0020,  // Initial handshake from client - TCP
    HELLO_ACK        = 0x0021,  // Handshake response from server - TCP
    DISCONNECT       = 0x0022,  // Graceful disconnect - TCP
    RECONNECT        = 0x0023,  // Reconnection request - TCP

    // Keepalive (0x00Fx)
    KEEPALIVE        = 0x00FF,  // Keepalive ping/pong - TCP
};

// ---------- Message payload structs ----------

struct MouseMove {
    int16_t dx;
    int16_t dy;
};

struct MouseMoveAbs {
    uint16_t x;
    uint16_t y;
};

struct MouseButton {
    uint8_t button;   // 0=left, 1=right, 2=middle, 3+=extra
    uint8_t pressed;  // 1=pressed, 0=released
};

struct MouseScroll {
    int16_t dx;  // horizontal
    int16_t dy;  // vertical
};

struct KeyDown {
    uint16_t scancode;   // USB HID usage code
    uint32_t modifiers;  // Bitmask: bit0=shift, bit1=ctrl, bit2=alt, bit3=gui/cmd
};

struct KeyUp {
    uint16_t scancode;
    uint32_t modifiers;
};

struct ScreenEnter {
    uint16_t x;  // Entry point x
    uint16_t y;  // Entry point y
    uint16_t w;  // Client screen width
    uint16_t h;  // Client screen height
};

struct ScreenLeave {};

enum class ClipboardFormat : uint8_t {
    TEXT = 0,
    HTML = 1,
    IMAGE_PNG = 2,
};

struct ClipboardUpdate {
    ClipboardFormat format;
    std::vector<uint8_t> data;
};

struct Hello {
    uint16_t version;
    uint16_t screen_w;
    uint16_t screen_h;
    std::string name;  // Machine name (UTF-8, max 255 bytes)
};

struct HelloAck {
    uint16_t version;
    uint8_t accepted;  // 1=accepted, 0=rejected
};

struct Disconnect {};
struct Reconnect {};
struct Keepalive {};

// Variant of all message types
using MessagePayload = std::variant<
    MouseMove, MouseMoveAbs, MouseButton, MouseScroll,
    KeyDown, KeyUp,
    ScreenEnter, ScreenLeave,
    ClipboardUpdate,
    Hello, HelloAck,
    Disconnect, Reconnect, Keepalive
>;

// ---------- Message header ----------

struct MessageHeader {
    MessageType type;
    uint16_t payload_length;
    uint32_t sequence;
};

// ---------- Complete message ----------

struct Message {
    MessageHeader header;
    MessagePayload payload;
};

// ---------- Serialization ----------

// Serialize a message header to bytes (big-endian)
void serialize_header(const MessageHeader& header, uint8_t* out);

// Deserialize a message header from bytes (big-endian)
std::optional<MessageHeader> deserialize_header(const uint8_t* data, size_t len);

// Serialize a message payload to bytes
std::vector<uint8_t> serialize_payload(const MessagePayload& payload);

// Deserialize a message payload from bytes
std::optional<MessagePayload> deserialize_payload(MessageType type, const uint8_t* data, size_t len);

// Serialize a complete message (header + payload)
std::vector<uint8_t> serialize_message(const Message& msg);

// Deserialize a complete message from bytes
// Returns the message and the number of bytes consumed
std::optional<std::pair<Message, size_t>> deserialize_message(const uint8_t* data, size_t len);

// Helper: create a message with auto-incrementing sequence number
Message make_message(MessagePayload payload);

// Reset sequence counter (for testing)
void reset_sequence();

} // namespace smouse
