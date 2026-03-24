#include "screen_layout.h"
#include <algorithm>

namespace smouse {

void ScreenLayout::set_server_screen(const ScreenRect& rect) {
    server_screen_ = rect;
}

void ScreenLayout::add_client(const ScreenInfo& info) {
    // Replace if client_id already exists
    remove_client(info.client_id);
    clients_.push_back(info);
}

void ScreenLayout::remove_client(const std::string& client_id) {
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [&](const ScreenInfo& s) { return s.client_id == client_id; }),
        clients_.end());
}

void ScreenLayout::clear_clients() {
    clients_.clear();
}

std::optional<std::string> ScreenLayout::check_edge(int32_t x, int32_t y) const {
    for (const auto& client : clients_) {
        switch (client.adjacent_edge) {
        case Edge::LEFT:
            if (x <= server_screen_.left()) {
                // Check dead zone (corners)
                if (y < server_screen_.top() + dead_zone_ ||
                    y > server_screen_.bottom() - dead_zone_)
                    continue;
                return client.client_id;
            }
            break;
        case Edge::RIGHT:
            if (x >= server_screen_.right() - 1) {
                if (y < server_screen_.top() + dead_zone_ ||
                    y > server_screen_.bottom() - dead_zone_)
                    continue;
                return client.client_id;
            }
            break;
        case Edge::TOP:
            if (y <= server_screen_.top()) {
                if (x < server_screen_.left() + dead_zone_ ||
                    x > server_screen_.right() - dead_zone_)
                    continue;
                return client.client_id;
            }
            break;
        case Edge::BOTTOM:
            if (y >= server_screen_.bottom() - 1) {
                if (x < server_screen_.left() + dead_zone_ ||
                    x > server_screen_.right() - dead_zone_)
                    continue;
                return client.client_id;
            }
            break;
        }
    }
    return std::nullopt;
}

std::pair<uint16_t, uint16_t> ScreenLayout::map_to_client(
    const std::string& client_id, Edge edge, float position) const {

    auto it = std::find_if(clients_.begin(), clients_.end(),
        [&](const ScreenInfo& s) { return s.client_id == client_id; });

    if (it == clients_.end()) return {0, 0};

    const auto& client = *it;
    uint16_t x = 0, y = 0;

    switch (edge) {
    case Edge::LEFT:
        x = static_cast<uint16_t>(client.rect.width - 1);
        y = static_cast<uint16_t>(position * client.rect.height);
        break;
    case Edge::RIGHT:
        x = 0;
        y = static_cast<uint16_t>(position * client.rect.height);
        break;
    case Edge::TOP:
        x = static_cast<uint16_t>(position * client.rect.width);
        y = static_cast<uint16_t>(client.rect.height - 1);
        break;
    case Edge::BOTTOM:
        x = static_cast<uint16_t>(position * client.rect.width);
        y = 0;
        break;
    }

    return {x, y};
}

std::optional<ScreenInfo> ScreenLayout::get_client(const std::string& client_id) const {
    auto it = std::find_if(clients_.begin(), clients_.end(),
        [&](const ScreenInfo& s) { return s.client_id == client_id; });
    if (it == clients_.end()) return std::nullopt;
    return *it;
}

} // namespace smouse
