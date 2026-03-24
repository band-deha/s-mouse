#pragma once

#include "protocol.h"
#include <functional>
#include <memory>
#include <vector>

namespace smouse {

// Abstract interface for capturing global input events
class InputCapture {
public:
    using EventCallback = std::function<void(Message msg)>;

    virtual ~InputCapture() = default;

    // Start capturing all mouse and keyboard events
    // When in capture mode, events are suppressed (not sent to local apps)
    virtual bool start(EventCallback callback) = 0;

    // Stop capturing
    virtual void stop() = 0;

    // Enable/disable event suppression
    // When suppressed, events are consumed and only forwarded via callback
    virtual void set_suppress(bool suppress) = 0;

    // Check if capturing
    virtual bool is_capturing() const = 0;

    // Set callback for emergency escape (e.g. Scroll Lock key)
    virtual void set_escape_callback(std::function<void()> cb) = 0;
};

// Abstract interface for injecting synthetic input events
class InputInject {
public:
    virtual ~InputInject() = default;

    // Inject a mouse move (absolute position)
    virtual void move_mouse(uint16_t x, uint16_t y) = 0;

    // Inject a mouse move (relative delta)
    virtual void move_mouse_relative(int16_t dx, int16_t dy) = 0;

    // Inject a mouse button press/release
    virtual void mouse_button(uint8_t button, bool pressed) = 0;

    // Inject a mouse scroll
    virtual void mouse_scroll(int16_t dx, int16_t dy) = 0;

    // Inject a key press
    virtual void key_down(uint16_t scancode, uint32_t modifiers) = 0;

    // Inject a key release
    virtual void key_up(uint16_t scancode, uint32_t modifiers) = 0;
};

// Abstract interface for getting screen geometry
class ScreenQuery {
public:
    struct DisplayInfo {
        int32_t x, y;
        uint32_t width, height;
        bool is_primary;
    };

    virtual ~ScreenQuery() = default;

    // Get all connected displays
    virtual std::vector<DisplayInfo> get_displays() = 0;

    // Get current cursor position
    virtual std::pair<int32_t, int32_t> get_cursor_position() = 0;
};

// Factory functions (implemented per-platform)
std::unique_ptr<InputCapture> create_input_capture();
std::unique_ptr<InputInject> create_input_inject();
std::unique_ptr<ScreenQuery> create_screen_query();

} // namespace smouse
