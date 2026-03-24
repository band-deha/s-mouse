#pragma once

#include <cstdint>

namespace smouse {

// Canonical scancode based on USB HID Usage Tables (Keyboard/Keypad page 0x07)
// Used as the wire format between platforms.
enum class ScanCode : uint16_t {
    NONE = 0x00,

    // Letters
    KEY_A = 0x04, KEY_B = 0x05, KEY_C = 0x06, KEY_D = 0x07,
    KEY_E = 0x08, KEY_F = 0x09, KEY_G = 0x0A, KEY_H = 0x0B,
    KEY_I = 0x0C, KEY_J = 0x0D, KEY_K = 0x0E, KEY_L = 0x0F,
    KEY_M = 0x10, KEY_N = 0x11, KEY_O = 0x12, KEY_P = 0x13,
    KEY_Q = 0x14, KEY_R = 0x15, KEY_S = 0x16, KEY_T = 0x17,
    KEY_U = 0x18, KEY_V = 0x19, KEY_W = 0x1A, KEY_X = 0x1B,
    KEY_Y = 0x1C, KEY_Z = 0x1D,

    // Numbers
    KEY_1 = 0x1E, KEY_2 = 0x1F, KEY_3 = 0x20, KEY_4 = 0x21,
    KEY_5 = 0x22, KEY_6 = 0x23, KEY_7 = 0x24, KEY_8 = 0x25,
    KEY_9 = 0x26, KEY_0 = 0x27,

    // Control keys
    KEY_ENTER     = 0x28,
    KEY_ESCAPE    = 0x29,
    KEY_BACKSPACE = 0x2A,
    KEY_TAB       = 0x2B,
    KEY_SPACE     = 0x2C,

    // Punctuation
    KEY_MINUS        = 0x2D,
    KEY_EQUAL        = 0x2E,
    KEY_LBRACKET     = 0x2F,
    KEY_RBRACKET     = 0x30,
    KEY_BACKSLASH    = 0x31,
    KEY_SEMICOLON    = 0x33,
    KEY_APOSTROPHE   = 0x34,
    KEY_GRAVE        = 0x35,
    KEY_COMMA        = 0x36,
    KEY_PERIOD       = 0x37,
    KEY_SLASH        = 0x38,

    // Function keys
    KEY_CAPSLOCK  = 0x39,
    KEY_F1  = 0x3A, KEY_F2  = 0x3B, KEY_F3  = 0x3C, KEY_F4  = 0x3D,
    KEY_F5  = 0x3E, KEY_F6  = 0x3F, KEY_F7  = 0x40, KEY_F8  = 0x41,
    KEY_F9  = 0x42, KEY_F10 = 0x43, KEY_F11 = 0x44, KEY_F12 = 0x45,

    // Navigation
    KEY_PRINT_SCREEN = 0x46,
    KEY_SCROLL_LOCK  = 0x47,
    KEY_PAUSE        = 0x48,
    KEY_INSERT       = 0x49,
    KEY_HOME         = 0x4A,
    KEY_PAGE_UP      = 0x4B,
    KEY_DELETE        = 0x4C,
    KEY_END          = 0x4D,
    KEY_PAGE_DOWN    = 0x4E,
    KEY_RIGHT        = 0x4F,
    KEY_LEFT         = 0x50,
    KEY_DOWN         = 0x51,
    KEY_UP           = 0x52,

    // Modifiers
    KEY_LCTRL  = 0xE0,
    KEY_LSHIFT = 0xE1,
    KEY_LALT   = 0xE2,
    KEY_LGUI   = 0xE3,  // Left Cmd (Mac) / Left Win (Windows)
    KEY_RCTRL  = 0xE4,
    KEY_RSHIFT = 0xE5,
    KEY_RALT   = 0xE6,
    KEY_RGUI   = 0xE7,  // Right Cmd (Mac) / Right Win (Windows)
};

// Modifier bitmask (used in KeyDown/KeyUp messages)
enum Modifier : uint32_t {
    MOD_NONE  = 0,
    MOD_SHIFT = 1 << 0,
    MOD_CTRL  = 1 << 1,
    MOD_ALT   = 1 << 2,
    MOD_GUI   = 1 << 3,  // Cmd on Mac, Win on Windows
};

// Platform-specific keycode conversion (implemented per-platform)
// macOS: src/platform/macos/macos_input_capture.mm
// Windows: src/platform/windows/win_input_capture.cpp

#ifdef __APPLE__
uint16_t macos_vk_to_hid(uint16_t vk);
uint16_t hid_to_macos_vk(uint16_t hid);
#endif

#ifdef _WIN32
uint16_t win_vk_to_hid(uint16_t vk);
uint16_t hid_to_win_vk(uint16_t hid);
#endif

} // namespace smouse
