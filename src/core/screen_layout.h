#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace smouse {

enum class Edge : uint8_t {
    LEFT = 0,
    RIGHT = 1,
    TOP = 2,
    BOTTOM = 3,
};

struct ScreenRect {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;

    int32_t left() const { return x; }
    int32_t right() const { return x + static_cast<int32_t>(width); }
    int32_t top() const { return y; }
    int32_t bottom() const { return y + static_cast<int32_t>(height); }
    bool contains(int32_t px, int32_t py) const {
        return px >= x && px < right() && py >= y && py < bottom();
    }
};

struct ScreenInfo {
    std::string name;          // Machine/client name
    std::string client_id;     // Unique client identifier
    ScreenRect rect;           // Screen dimensions
    Edge adjacent_edge;        // Which edge of server this screen is on
};

struct EdgeHit {
    Edge edge;
    float position;  // 0.0 - 1.0 normalized position along the edge
};

class ScreenLayout {
public:
    // Set the server screen rect
    void set_server_screen(const ScreenRect& rect);
    const ScreenRect& server_screen() const { return server_screen_; }

    // Add/remove client screens
    void add_client(const ScreenInfo& info);
    void remove_client(const std::string& client_id);
    void clear_clients();

    // Set which edge of the server a client is on
    void set_client_edge(const std::string& client_id, Edge edge);

    // Check if a point hits any screen edge that has a client
    // Returns the client_id if the cursor should transition
    std::optional<std::string> check_edge(int32_t x, int32_t y) const;

    // Map a position from server edge to client entry point
    // position is normalized 0.0-1.0 along the edge
    std::pair<uint16_t, uint16_t> map_to_client(
        const std::string& client_id, Edge edge, float position) const;

    // Get client info
    std::optional<ScreenInfo> get_client(const std::string& client_id) const;

    const std::vector<ScreenInfo>& clients() const { return clients_; }

private:
    ScreenRect server_screen_{};
    std::vector<ScreenInfo> clients_;
    int32_t dead_zone_ = 5;  // pixels from corner to ignore
};

} // namespace smouse
