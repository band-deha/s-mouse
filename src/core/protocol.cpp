#include "protocol.h"

#include <atomic>
#include <stdexcept>

namespace smouse {

namespace {

std::atomic<uint32_t> g_sequence{0};

// Big-endian helpers
void write_u16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v);
}

void write_u32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v >> 24);
    p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >> 8);
    p[3] = static_cast<uint8_t>(v);
}

void write_i16(uint8_t* p, int16_t v) {
    write_u16(p, static_cast<uint16_t>(v));
}

uint16_t read_u16(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

int16_t read_i16(const uint8_t* p) {
    return static_cast<int16_t>(read_u16(p));
}

uint32_t read_u32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
           static_cast<uint32_t>(p[3]);
}

} // anonymous namespace

void serialize_header(const MessageHeader& header, uint8_t* out) {
    write_u16(out, static_cast<uint16_t>(header.type));
    write_u16(out + 2, header.payload_length);
    write_u32(out + 4, header.sequence);
}

std::optional<MessageHeader> deserialize_header(const uint8_t* data, size_t len) {
    if (len < HEADER_SIZE) return std::nullopt;

    MessageHeader h;
    h.type = static_cast<MessageType>(read_u16(data));
    h.payload_length = read_u16(data + 2);
    h.sequence = read_u32(data + 4);

    // payload_length is uint16_t (max 65535), which is always < MAX_PAYLOAD_SIZE
    // For future-proofing, keep the check but cast appropriately
    if (static_cast<size_t>(h.payload_length) > MAX_PAYLOAD_SIZE) return std::nullopt;

    return h;
}

std::vector<uint8_t> serialize_payload(const MessagePayload& payload) {
    std::vector<uint8_t> buf;

    std::visit([&buf](const auto& p) {
        using T = std::decay_t<decltype(p)>;

        if constexpr (std::is_same_v<T, MouseMove>) {
            buf.resize(4);
            write_i16(buf.data(), p.dx);
            write_i16(buf.data() + 2, p.dy);
        }
        else if constexpr (std::is_same_v<T, MouseMoveAbs>) {
            buf.resize(4);
            write_u16(buf.data(), p.x);
            write_u16(buf.data() + 2, p.y);
        }
        else if constexpr (std::is_same_v<T, MouseButton>) {
            buf.resize(2);
            buf[0] = p.button;
            buf[1] = p.pressed;
        }
        else if constexpr (std::is_same_v<T, MouseScroll>) {
            buf.resize(4);
            write_i16(buf.data(), p.dx);
            write_i16(buf.data() + 2, p.dy);
        }
        else if constexpr (std::is_same_v<T, KeyDown>) {
            buf.resize(6);
            write_u16(buf.data(), p.scancode);
            write_u32(buf.data() + 2, p.modifiers);
        }
        else if constexpr (std::is_same_v<T, KeyUp>) {
            buf.resize(6);
            write_u16(buf.data(), p.scancode);
            write_u32(buf.data() + 2, p.modifiers);
        }
        else if constexpr (std::is_same_v<T, ScreenEnter>) {
            buf.resize(8);
            write_u16(buf.data(), p.x);
            write_u16(buf.data() + 2, p.y);
            write_u16(buf.data() + 4, p.w);
            write_u16(buf.data() + 6, p.h);
        }
        else if constexpr (std::is_same_v<T, ScreenLeave>) {
            // Empty payload
        }
        else if constexpr (std::is_same_v<T, ClipboardUpdate>) {
            // format(1) + length(4) + data
            buf.resize(5 + p.data.size());
            buf[0] = static_cast<uint8_t>(p.format);
            write_u32(buf.data() + 1, static_cast<uint32_t>(p.data.size()));
            std::memcpy(buf.data() + 5, p.data.data(), p.data.size());
        }
        else if constexpr (std::is_same_v<T, Hello>) {
            // version(2) + screen_w(2) + screen_h(2) + name_len(1) + name
            uint8_t name_len = static_cast<uint8_t>(
                std::min(p.name.size(), size_t{255}));
            buf.resize(7 + name_len);
            write_u16(buf.data(), p.version);
            write_u16(buf.data() + 2, p.screen_w);
            write_u16(buf.data() + 4, p.screen_h);
            buf[6] = name_len;
            std::memcpy(buf.data() + 7, p.name.data(), name_len);
        }
        else if constexpr (std::is_same_v<T, HelloAck>) {
            buf.resize(3);
            write_u16(buf.data(), p.version);
            buf[2] = p.accepted;
        }
        else if constexpr (std::is_same_v<T, Disconnect>) {
            // Empty payload
        }
        else if constexpr (std::is_same_v<T, Reconnect>) {
            // Empty payload
        }
        else if constexpr (std::is_same_v<T, Keepalive>) {
            // Empty payload
        }
    }, payload);

    return buf;
}

std::optional<MessagePayload> deserialize_payload(MessageType type, const uint8_t* data, size_t len) {
    switch (type) {
    case MessageType::MOUSE_MOVE: {
        if (len < 4) return std::nullopt;
        return MouseMove{read_i16(data), read_i16(data + 2)};
    }
    case MessageType::MOUSE_MOVE_ABS: {
        if (len < 4) return std::nullopt;
        return MouseMoveAbs{read_u16(data), read_u16(data + 2)};
    }
    case MessageType::MOUSE_BUTTON: {
        if (len < 2) return std::nullopt;
        return MouseButton{data[0], data[1]};
    }
    case MessageType::MOUSE_SCROLL: {
        if (len < 4) return std::nullopt;
        return MouseScroll{read_i16(data), read_i16(data + 2)};
    }
    case MessageType::KEY_DOWN: {
        if (len < 6) return std::nullopt;
        return KeyDown{read_u16(data), read_u32(data + 2)};
    }
    case MessageType::KEY_UP: {
        if (len < 6) return std::nullopt;
        return KeyUp{read_u16(data), read_u32(data + 2)};
    }
    case MessageType::SCREEN_ENTER: {
        if (len < 8) return std::nullopt;
        return ScreenEnter{read_u16(data), read_u16(data + 2),
                           read_u16(data + 4), read_u16(data + 6)};
    }
    case MessageType::SCREEN_LEAVE:
        return ScreenLeave{};
    case MessageType::CLIPBOARD_UPDATE: {
        if (len < 5) return std::nullopt;
        auto format = static_cast<ClipboardFormat>(data[0]);
        uint32_t data_len = read_u32(data + 1);
        if (len < 5 + data_len) return std::nullopt;
        ClipboardUpdate cb;
        cb.format = format;
        cb.data.assign(data + 5, data + 5 + data_len);
        return cb;
    }
    case MessageType::HELLO: {
        if (len < 7) return std::nullopt;
        Hello h;
        h.version = read_u16(data);
        h.screen_w = read_u16(data + 2);
        h.screen_h = read_u16(data + 4);
        uint8_t name_len = data[6];
        if (len < 7u + name_len) return std::nullopt;
        h.name.assign(reinterpret_cast<const char*>(data + 7), name_len);
        return h;
    }
    case MessageType::HELLO_ACK: {
        if (len < 3) return std::nullopt;
        return HelloAck{read_u16(data), data[2]};
    }
    case MessageType::DISCONNECT:
        return Disconnect{};
    case MessageType::RECONNECT:
        return Reconnect{};
    case MessageType::KEEPALIVE:
        return Keepalive{};
    }

    return std::nullopt;
}

std::vector<uint8_t> serialize_message(const Message& msg) {
    auto payload_bytes = serialize_payload(msg.payload);

    std::vector<uint8_t> buf(HEADER_SIZE + payload_bytes.size());

    MessageHeader h = msg.header;
    h.payload_length = static_cast<uint16_t>(payload_bytes.size());

    serialize_header(h, buf.data());
    if (!payload_bytes.empty()) {
        std::memcpy(buf.data() + HEADER_SIZE, payload_bytes.data(), payload_bytes.size());
    }

    return buf;
}

std::optional<std::pair<Message, size_t>> deserialize_message(const uint8_t* data, size_t len) {
    auto header_opt = deserialize_header(data, len);
    if (!header_opt) return std::nullopt;

    auto& header = *header_opt;
    size_t total = HEADER_SIZE + header.payload_length;
    if (len < total) return std::nullopt;

    auto payload_opt = deserialize_payload(
        header.type, data + HEADER_SIZE, header.payload_length);
    if (!payload_opt) return std::nullopt;

    Message msg;
    msg.header = header;
    msg.payload = std::move(*payload_opt);

    return std::make_pair(std::move(msg), total);
}

Message make_message(MessagePayload payload) {
    auto payload_bytes = serialize_payload(payload);

    Message msg;
    msg.payload = std::move(payload);

    // Determine type from variant
    std::visit([&msg](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, MouseMove>)       msg.header.type = MessageType::MOUSE_MOVE;
        else if constexpr (std::is_same_v<T, MouseMoveAbs>) msg.header.type = MessageType::MOUSE_MOVE_ABS;
        else if constexpr (std::is_same_v<T, MouseButton>) msg.header.type = MessageType::MOUSE_BUTTON;
        else if constexpr (std::is_same_v<T, MouseScroll>) msg.header.type = MessageType::MOUSE_SCROLL;
        else if constexpr (std::is_same_v<T, KeyDown>)    msg.header.type = MessageType::KEY_DOWN;
        else if constexpr (std::is_same_v<T, KeyUp>)      msg.header.type = MessageType::KEY_UP;
        else if constexpr (std::is_same_v<T, ScreenEnter>) msg.header.type = MessageType::SCREEN_ENTER;
        else if constexpr (std::is_same_v<T, ScreenLeave>) msg.header.type = MessageType::SCREEN_LEAVE;
        else if constexpr (std::is_same_v<T, ClipboardUpdate>) msg.header.type = MessageType::CLIPBOARD_UPDATE;
        else if constexpr (std::is_same_v<T, Hello>)      msg.header.type = MessageType::HELLO;
        else if constexpr (std::is_same_v<T, HelloAck>)   msg.header.type = MessageType::HELLO_ACK;
        else if constexpr (std::is_same_v<T, Disconnect>)  msg.header.type = MessageType::DISCONNECT;
        else if constexpr (std::is_same_v<T, Reconnect>)   msg.header.type = MessageType::RECONNECT;
        else if constexpr (std::is_same_v<T, Keepalive>)   msg.header.type = MessageType::KEEPALIVE;
    }, msg.payload);

    msg.header.payload_length = static_cast<uint16_t>(payload_bytes.size());
    msg.header.sequence = g_sequence.fetch_add(1, std::memory_order_relaxed);

    return msg;
}

void reset_sequence() {
    g_sequence.store(0, std::memory_order_relaxed);
}

} // namespace smouse
