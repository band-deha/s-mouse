#import "../platform.h"
#import "core/keymap.h"
#import <ApplicationServices/ApplicationServices.h>

namespace smouse {

class MacOSInputInject : public InputInject {
public:
    void move_mouse(uint16_t x, uint16_t y) override {
        CGPoint point = CGPointMake(x, y);
        CGWarpMouseCursorPosition(point);
        // Associate the mouse with the new position to avoid delta issues
        CGAssociateMouseAndMouseCursorPosition(true);
    }

    void move_mouse_relative(int16_t dx, int16_t dy) override {
        CGEventRef event = CGEventCreate(nullptr);
        CGPoint current = CGEventGetLocation(event);
        CFRelease(event);

        CGPoint newPos = CGPointMake(current.x + dx, current.y + dy);
        CGWarpMouseCursorPosition(newPos);
        CGAssociateMouseAndMouseCursorPosition(true);
    }

    void mouse_button(uint8_t button, bool pressed) override {
        CGEventRef event = CGEventCreate(nullptr);
        CGPoint pos = CGEventGetLocation(event);
        CFRelease(event);

        CGEventType type;
        CGMouseButton cgButton;

        switch (button) {
        case 0: // Left
            type = pressed ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
            cgButton = kCGMouseButtonLeft;
            break;
        case 1: // Right
            type = pressed ? kCGEventRightMouseDown : kCGEventRightMouseUp;
            cgButton = kCGMouseButtonRight;
            break;
        default: // Other
            type = pressed ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
            cgButton = static_cast<CGMouseButton>(button);
            break;
        }

        CGEventRef mouseEvent = CGEventCreateMouseEvent(nullptr, type, pos, cgButton);
        CGEventPost(kCGHIDEventTap, mouseEvent);
        CFRelease(mouseEvent);
    }

    void mouse_scroll(int16_t dx, int16_t dy) override {
        CGEventRef event = CGEventCreateScrollWheelEvent(
            nullptr, kCGScrollEventUnitLine, 2,
            static_cast<int32_t>(dy), static_cast<int32_t>(dx));
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }

    void key_down(uint16_t scancode, uint32_t /*modifiers*/) override {
        uint16_t vk = hid_to_macos_vk(scancode);
        if (vk == 0xFFFF) return;

        CGEventRef event = CGEventCreateKeyboardEvent(nullptr, vk, true);
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }

    void key_up(uint16_t scancode, uint32_t /*modifiers*/) override {
        uint16_t vk = hid_to_macos_vk(scancode);
        if (vk == 0xFFFF) return;

        CGEventRef event = CGEventCreateKeyboardEvent(nullptr, vk, false);
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }
};

std::unique_ptr<InputInject> create_input_inject() {
    return std::make_unique<MacOSInputInject>();
}

} // namespace smouse
