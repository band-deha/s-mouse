#import "macos_input_capture.h"
#import "core/keymap.h"
#import <ApplicationServices/ApplicationServices.h>

namespace smouse {

// macOS virtual keycode to USB HID scancode mapping
// Based on Events.h (Carbon) virtual key codes
static const uint16_t vk_to_hid_table[] = {
    // 0x00-0x0F
    0x04, // kVK_ANSI_A = 0x00 -> KEY_A
    0x16, // kVK_ANSI_S = 0x01 -> KEY_S
    0x07, // kVK_ANSI_D = 0x02 -> KEY_D
    0x09, // kVK_ANSI_F = 0x03 -> KEY_F
    0x0B, // kVK_ANSI_H = 0x04 -> KEY_H
    0x0A, // kVK_ANSI_G = 0x05 -> KEY_G
    0x1D, // kVK_ANSI_Z = 0x06 -> KEY_Z
    0x1B, // kVK_ANSI_X = 0x07 -> KEY_X
    0x06, // kVK_ANSI_C = 0x08 -> KEY_C
    0x19, // kVK_ANSI_V = 0x09 -> KEY_V
    0x64, // kVK_ISO_Section = 0x0A -> (non-US)
    0x05, // kVK_ANSI_B = 0x0B -> KEY_B
    0x14, // kVK_ANSI_Q = 0x0C -> KEY_Q
    0x1A, // kVK_ANSI_W = 0x0D -> KEY_W
    0x08, // kVK_ANSI_E = 0x0E -> KEY_E
    0x15, // kVK_ANSI_R = 0x0F -> KEY_R
    // 0x10-0x1F
    0x1C, // kVK_ANSI_Y = 0x10 -> KEY_Y
    0x17, // kVK_ANSI_T = 0x11 -> KEY_T
    0x1E, // kVK_ANSI_1 = 0x12 -> KEY_1
    0x1F, // kVK_ANSI_2 = 0x13 -> KEY_2
    0x20, // kVK_ANSI_3 = 0x14 -> KEY_3
    0x21, // kVK_ANSI_4 = 0x15 -> KEY_4
    0x23, // kVK_ANSI_6 = 0x16 -> KEY_6
    0x22, // kVK_ANSI_5 = 0x17 -> KEY_5
    0x2E, // kVK_ANSI_Equal = 0x18 -> KEY_EQUAL
    0x26, // kVK_ANSI_9 = 0x19 -> KEY_9
    0x24, // kVK_ANSI_7 = 0x1A -> KEY_7
    0x2D, // kVK_ANSI_Minus = 0x1B -> KEY_MINUS
    0x25, // kVK_ANSI_8 = 0x1C -> KEY_8
    0x27, // kVK_ANSI_0 = 0x1D -> KEY_0
    0x30, // kVK_ANSI_RightBracket = 0x1E -> KEY_RBRACKET
    0x12, // kVK_ANSI_O = 0x1F -> KEY_O
    // 0x20-0x2F
    0x18, // kVK_ANSI_U = 0x20 -> KEY_U
    0x2F, // kVK_ANSI_LeftBracket = 0x21 -> KEY_LBRACKET
    0x0C, // kVK_ANSI_I = 0x22 -> KEY_I
    0x13, // kVK_ANSI_P = 0x23 -> KEY_P
    0x28, // kVK_Return = 0x24 -> KEY_ENTER
    0x0F, // kVK_ANSI_L = 0x25 -> KEY_L
    0x0D, // kVK_ANSI_J = 0x26 -> KEY_J
    0x34, // kVK_ANSI_Quote = 0x27 -> KEY_APOSTROPHE
    0x0E, // kVK_ANSI_K = 0x28 -> KEY_K
    0x33, // kVK_ANSI_Semicolon = 0x29 -> KEY_SEMICOLON
    0x31, // kVK_ANSI_Backslash = 0x2A -> KEY_BACKSLASH
    0x36, // kVK_ANSI_Comma = 0x2B -> KEY_COMMA
    0x38, // kVK_ANSI_Slash = 0x2C -> KEY_SLASH
    0x11, // kVK_ANSI_N = 0x2D -> KEY_N
    0x10, // kVK_ANSI_M = 0x2E -> KEY_M
    0x37, // kVK_ANSI_Period = 0x2F -> KEY_PERIOD
    // 0x30-0x3F
    0x2B, // kVK_Tab = 0x30 -> KEY_TAB
    0x2C, // kVK_Space = 0x31 -> KEY_SPACE
    0x35, // kVK_ANSI_Grave = 0x32 -> KEY_GRAVE
    0x2A, // kVK_Delete = 0x33 -> KEY_BACKSPACE
    0x00, // 0x34 -> unused
    0x29, // kVK_Escape = 0x35 -> KEY_ESCAPE
    0xE7, // kVK_RightCommand = 0x36 -> KEY_RGUI
    0xE3, // kVK_Command = 0x37 -> KEY_LGUI
    0xE1, // kVK_Shift = 0x38 -> KEY_LSHIFT
    0x39, // kVK_CapsLock = 0x39 -> KEY_CAPSLOCK
    0xE2, // kVK_Option = 0x3A -> KEY_LALT
    0xE0, // kVK_Control = 0x3B -> KEY_LCTRL
    0xE5, // kVK_RightShift = 0x3C -> KEY_RSHIFT
    0xE6, // kVK_RightOption = 0x3D -> KEY_RALT
    0xE4, // kVK_RightControl = 0x3E -> KEY_RCTRL
    0x00, // kVK_Function = 0x3F -> (no HID equivalent)
    // 0x40-0x4F
    0x00, // kVK_F17 = 0x40
    0x00, // 0x41 (Keypad .)
    0x00, // 0x42
    0x55, // kVK_ANSI_KeypadMultiply = 0x43
    0x00, // 0x44
    0x57, // kVK_ANSI_KeypadPlus = 0x45
    0x00, // 0x46
    0x53, // kVK_ANSI_KeypadClear = 0x47 -> NumLock
    0x00, // 0x48 (Volume Up)
    0x00, // 0x49 (Volume Down)
    0x00, // 0x4A (Mute)
    0x54, // kVK_ANSI_KeypadDivide = 0x4B
    0x58, // kVK_ANSI_KeypadEnter = 0x4C
    0x00, // 0x4D
    0x56, // kVK_ANSI_KeypadMinus = 0x4E
    0x00, // 0x4F
    // 0x50-0x5F
    0x00, // 0x50
    0x67, // kVK_ANSI_KeypadEquals = 0x51
    0x62, // kVK_ANSI_Keypad0 = 0x52
    0x59, // kVK_ANSI_Keypad1 = 0x53
    0x5A, // kVK_ANSI_Keypad2 = 0x54
    0x5B, // kVK_ANSI_Keypad3 = 0x55
    0x5C, // kVK_ANSI_Keypad4 = 0x56
    0x5D, // kVK_ANSI_Keypad5 = 0x57
    0x5E, // kVK_ANSI_Keypad6 = 0x58
    0x5F, // kVK_ANSI_Keypad7 = 0x59
    0x00, // 0x5A
    0x60, // kVK_ANSI_Keypad8 = 0x5B
    0x61, // kVK_ANSI_Keypad9 = 0x5C
    0x00, // 0x5D
    0x00, // 0x5E
    0x00, // 0x5F
    // 0x60-0x6F
    0x44, // kVK_F5 = 0x60 -> KEY_F5
    0x45, // kVK_F6 = 0x61 -> KEY_F6
    0x40, // kVK_F7 = 0x62 -> KEY_F7
    0x3E, // kVK_F3 = 0x63 -> KEY_F3
    0x41, // kVK_F8 = 0x64 -> KEY_F8
    0x42, // kVK_F9 = 0x65 -> KEY_F9
    0x00, // 0x66
    0x43, // kVK_F11 = 0x67 -> KEY_F11
    0x00, // 0x68
    0x46, // kVK_F13 = 0x69 -> KEY_PRINT_SCREEN
    0x00, // 0x6A
    0x47, // kVK_F14 = 0x6B -> KEY_SCROLL_LOCK
    0x00, // 0x6C
    0x3F, // kVK_F10 = 0x6D -> KEY_F10
    0x00, // 0x6E
    0x44, // kVK_F12 = 0x6F -> KEY_F12 (actually 0x45 for F12)
    // 0x70-0x7F
    0x00, // 0x70
    0x48, // kVK_F15 = 0x71 -> KEY_PAUSE
    0x49, // kVK_Help = 0x72 -> KEY_INSERT
    0x4A, // kVK_Home = 0x73 -> KEY_HOME
    0x4B, // kVK_PageUp = 0x74 -> KEY_PAGE_UP
    0x4C, // kVK_ForwardDelete = 0x75 -> KEY_DELETE
    0x3D, // kVK_F4 = 0x76 -> KEY_F4
    0x4D, // kVK_End = 0x77 -> KEY_END
    0x3C, // kVK_F2 = 0x78 -> KEY_F2
    0x4E, // kVK_PageDown = 0x79 -> KEY_PAGE_DOWN
    0x3A, // kVK_F1 = 0x7A -> KEY_F1
    0x50, // kVK_LeftArrow = 0x7B -> KEY_LEFT
    0x4F, // kVK_RightArrow = 0x7C -> KEY_RIGHT
    0x51, // kVK_DownArrow = 0x7D -> KEY_DOWN
    0x52, // kVK_UpArrow = 0x7E -> KEY_UP
    0x00, // 0x7F
};

static constexpr size_t VK_TABLE_SIZE = sizeof(vk_to_hid_table) / sizeof(vk_to_hid_table[0]);

uint16_t macos_vk_to_hid(uint16_t vk) {
    if (vk < VK_TABLE_SIZE) return vk_to_hid_table[vk];
    return 0;
}

uint16_t hid_to_macos_vk(uint16_t hid) {
    for (size_t vk = 0; vk < VK_TABLE_SIZE; vk++) {
        if (vk_to_hid_table[vk] == hid) return static_cast<uint16_t>(vk);
    }
    return 0xFFFF;
}

static uint32_t cg_flags_to_modifiers(CGEventFlags flags) {
    uint32_t mods = 0;
    if (flags & kCGEventFlagMaskShift)     mods |= MOD_SHIFT;
    if (flags & kCGEventFlagMaskControl)   mods |= MOD_CTRL;
    if (flags & kCGEventFlagMaskAlternate) mods |= MOD_ALT;
    if (flags & kCGEventFlagMaskCommand)   mods |= MOD_GUI;
    return mods;
}

MacOSInputCapture::MacOSInputCapture() = default;

MacOSInputCapture::~MacOSInputCapture() {
    stop();
}

bool MacOSInputCapture::start(EventCallback callback) {
    if (capturing_) return false;

    callback_ = std::move(callback);

    CGEventMask mask =
        CGEventMaskBit(kCGEventMouseMoved) |
        CGEventMaskBit(kCGEventLeftMouseDown) |
        CGEventMaskBit(kCGEventLeftMouseUp) |
        CGEventMaskBit(kCGEventRightMouseDown) |
        CGEventMaskBit(kCGEventRightMouseUp) |
        CGEventMaskBit(kCGEventOtherMouseDown) |
        CGEventMaskBit(kCGEventOtherMouseUp) |
        CGEventMaskBit(kCGEventLeftMouseDragged) |
        CGEventMaskBit(kCGEventRightMouseDragged) |
        CGEventMaskBit(kCGEventOtherMouseDragged) |
        CGEventMaskBit(kCGEventScrollWheel) |
        CGEventMaskBit(kCGEventKeyDown) |
        CGEventMaskBit(kCGEventKeyUp) |
        CGEventMaskBit(kCGEventFlagsChanged);

    event_tap_ = CGEventTapCreate(
        kCGHIDEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,  // Active tap - can suppress events
        mask,
        event_callback,
        this
    );

    if (!event_tap_) {
        // Likely missing Accessibility permission
        return false;
    }

    run_loop_source_ = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault, event_tap_, 0);

    if (!run_loop_source_) {
        CFRelease(event_tap_);
        event_tap_ = nullptr;
        return false;
    }

    capturing_ = true;

    // Run the event tap on a dedicated thread
    tap_thread_ = std::thread([this]() {
        run_loop_ = CFRunLoopGetCurrent();
        CFRunLoopAddSource(run_loop_, run_loop_source_, kCFRunLoopCommonModes);
        CGEventTapEnable(event_tap_, true);
        CFRunLoopRun();
    });

    return true;
}

void MacOSInputCapture::stop() {
    if (!capturing_) return;
    capturing_ = false;

    if (event_tap_) {
        CGEventTapEnable(event_tap_, false);
    }

    if (run_loop_) {
        CFRunLoopStop(run_loop_);
    }

    if (tap_thread_.joinable()) {
        tap_thread_.join();
    }

    if (run_loop_source_) {
        CFRelease(run_loop_source_);
        run_loop_source_ = nullptr;
    }

    if (event_tap_) {
        CFRelease(event_tap_);
        event_tap_ = nullptr;
    }

    run_loop_ = nullptr;
}

void MacOSInputCapture::set_suppress(bool suppress) {
    suppress_ = suppress;
}

bool MacOSInputCapture::is_capturing() const {
    return capturing_;
}

void MacOSInputCapture::set_escape_callback(std::function<void()> cb) {
    escape_callback_ = std::move(cb);
}

CGEventRef MacOSInputCapture::event_callback(
    CGEventTapProxy /*proxy*/, CGEventType type,
    CGEventRef event, void* refcon) {

    auto* self = static_cast<MacOSInputCapture*>(refcon);

    // Re-enable tap if it was disabled (OS disables taps that take too long)
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        if (self->event_tap_) {
            CGEventTapEnable(self->event_tap_, true);
        }
        return event;
    }

    return self->handle_event(type, event);
}

CGEventRef MacOSInputCapture::handle_event(CGEventType type, CGEventRef event) {
    if (!callback_) return event;

    CGPoint location = CGEventGetLocation(event);

    switch (type) {
    case kCGEventMouseMoved:
    case kCGEventLeftMouseDragged:
    case kCGEventRightMouseDragged:
    case kCGEventOtherMouseDragged: {
        // Send both absolute position and relative delta
        int64_t dx = CGEventGetIntegerValueField(event, kCGMouseEventDeltaX);
        int64_t dy = CGEventGetIntegerValueField(event, kCGMouseEventDeltaY);

        auto abs_msg = make_message(MouseMoveAbs{
            static_cast<uint16_t>(location.x),
            static_cast<uint16_t>(location.y)
        });
        callback_(std::move(abs_msg));

        if (dx != 0 || dy != 0) {
            auto rel_msg = make_message(MouseMove{
                static_cast<int16_t>(dx),
                static_cast<int16_t>(dy)
            });
            callback_(std::move(rel_msg));
        }
        break;
    }

    case kCGEventLeftMouseDown:
        callback_(make_message(MouseButton{0, 1}));
        break;
    case kCGEventLeftMouseUp:
        callback_(make_message(MouseButton{0, 0}));
        break;
    case kCGEventRightMouseDown:
        callback_(make_message(MouseButton{1, 1}));
        break;
    case kCGEventRightMouseUp:
        callback_(make_message(MouseButton{1, 0}));
        break;
    case kCGEventOtherMouseDown: {
        int64_t btn = CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber);
        callback_(make_message(MouseButton{static_cast<uint8_t>(btn), 1}));
        break;
    }
    case kCGEventOtherMouseUp: {
        int64_t btn = CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber);
        callback_(make_message(MouseButton{static_cast<uint8_t>(btn), 0}));
        break;
    }

    case kCGEventScrollWheel: {
        int64_t dy = CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1);
        int64_t dx = CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis2);
        callback_(make_message(MouseScroll{
            static_cast<int16_t>(dx),
            static_cast<int16_t>(dy)
        }));
        break;
    }

    case kCGEventKeyDown: {
        uint16_t vk = static_cast<uint16_t>(
            CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));

        // F15 (0x71) = Scroll Lock equivalent on Mac - emergency escape
        if (vk == 0x71 && suppress_) {
            suppress_ = false;
            if (escape_callback_) escape_callback_();
            return nullptr; // consume key
        }

        uint16_t hid = macos_vk_to_hid(vk);
        uint32_t mods = cg_flags_to_modifiers(CGEventGetFlags(event));
        callback_(make_message(KeyDown{hid, mods}));
        break;
    }

    case kCGEventKeyUp: {
        uint16_t vk = static_cast<uint16_t>(
            CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
        uint16_t hid = macos_vk_to_hid(vk);
        uint32_t mods = cg_flags_to_modifiers(CGEventGetFlags(event));
        callback_(make_message(KeyUp{hid, mods}));
        break;
    }

    case kCGEventFlagsChanged: {
        // Modifier-only key change (e.g., pressing Shift alone)
        uint16_t vk = static_cast<uint16_t>(
            CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
        uint16_t hid = macos_vk_to_hid(vk);
        CGEventFlags flags = CGEventGetFlags(event);
        uint32_t mods = cg_flags_to_modifiers(flags);

        // Determine if key is pressed or released based on the flag state
        bool pressed = false;
        switch (vk) {
        case 0x38: case 0x3C: pressed = (flags & kCGEventFlagMaskShift) != 0; break;
        case 0x3B: case 0x3E: pressed = (flags & kCGEventFlagMaskControl) != 0; break;
        case 0x3A: case 0x3D: pressed = (flags & kCGEventFlagMaskAlternate) != 0; break;
        case 0x37: case 0x36: pressed = (flags & kCGEventFlagMaskCommand) != 0; break;
        case 0x39: pressed = (flags & kCGEventFlagMaskAlphaShift) != 0; break;
        default: pressed = true; break;
        }

        if (pressed) {
            callback_(make_message(KeyDown{hid, mods}));
        } else {
            callback_(make_message(KeyUp{hid, mods}));
        }
        break;
    }

    default:
        break;
    }

    // Suppress event if in suppress mode (forwarding to remote client)
    if (suppress_) {
        return nullptr;  // Event consumed, not delivered locally
    }

    return event;
}

// Factory
std::unique_ptr<InputCapture> create_input_capture() {
    return std::make_unique<MacOSInputCapture>();
}

} // namespace smouse
