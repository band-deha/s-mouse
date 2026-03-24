#ifdef _WIN32

#include "../platform.h"
#include "core/keymap.h"

#include <windows.h>

namespace smouse {

class WinInputInject : public InputInject {
public:
    void move_mouse(uint16_t x, uint16_t y) override {
        // Convert to normalized absolute coordinates (0-65535)
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);

        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>(x * 65535 / screen_w);
        input.mi.dy = static_cast<LONG>(y * 65535 / screen_h);
        input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
        SendInput(1, &input, sizeof(INPUT));
    }

    void move_mouse_relative(int16_t dx, int16_t dy) override {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dx = dx;
        input.mi.dy = dy;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &input, sizeof(INPUT));
    }

    void mouse_button(uint8_t button, bool pressed) override {
        INPUT input{};
        input.type = INPUT_MOUSE;

        switch (button) {
        case 0: // Left
            input.mi.dwFlags = pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            break;
        case 1: // Right
            input.mi.dwFlags = pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
            break;
        case 2: // Middle
            input.mi.dwFlags = pressed ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
            break;
        default:
            input.mi.dwFlags = pressed ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
            input.mi.mouseData = (button == 3) ? XBUTTON1 : XBUTTON2;
            break;
        }

        SendInput(1, &input, sizeof(INPUT));
    }

    void mouse_scroll(int16_t dx, int16_t dy) override {
        if (dy != 0) {
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_WHEEL;
            input.mi.mouseData = static_cast<DWORD>(dy * WHEEL_DELTA);
            SendInput(1, &input, sizeof(INPUT));
        }
        if (dx != 0) {
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
            input.mi.mouseData = static_cast<DWORD>(dx * WHEEL_DELTA);
            SendInput(1, &input, sizeof(INPUT));
        }
    }

    void key_down(uint16_t scancode, uint32_t /*modifiers*/) override {
        uint16_t vk = hid_to_win_vk(scancode);
        if (vk == 0) return;

        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
        SendInput(1, &input, sizeof(INPUT));
    }

    void key_up(uint16_t scancode, uint32_t /*modifiers*/) override {
        uint16_t vk = hid_to_win_vk(scancode);
        if (vk == 0) return;

        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    }
};

std::unique_ptr<InputInject> create_input_inject() {
    return std::make_unique<WinInputInject>();
}

} // namespace smouse

#endif // _WIN32
