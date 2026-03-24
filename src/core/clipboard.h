#pragma once

#include "protocol.h"
#include <functional>
#include <memory>
#include <vector>

namespace smouse {

// Platform-independent clipboard interface
class ClipboardMonitor {
public:
    using ChangeCallback = std::function<void(ClipboardFormat format, std::vector<uint8_t> data)>;

    virtual ~ClipboardMonitor() = default;

    // Start monitoring clipboard changes
    virtual void start(ChangeCallback callback) = 0;

    // Stop monitoring
    virtual void stop() = 0;

    // Get current clipboard content
    virtual std::optional<std::pair<ClipboardFormat, std::vector<uint8_t>>> get_content() = 0;

    // Set clipboard content
    virtual void set_content(ClipboardFormat format, const std::vector<uint8_t>& data) = 0;
};

// Factory function (implemented per-platform)
std::unique_ptr<ClipboardMonitor> create_clipboard_monitor();

} // namespace smouse
