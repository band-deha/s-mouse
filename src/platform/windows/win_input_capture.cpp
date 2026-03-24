#ifdef _WIN32

#include "../platform.h"
#include "core/keymap.h"

#include <windows.h>
#include <atomic>
#include <thread>

namespace smouse {

// Windows virtual key to USB HID mapping
uint16_t win_vk_to_hid(uint16_t vk) {
    switch (vk) {
    case 'A': return 0x04; case 'B': return 0x05; case 'C': return 0x06;
    case 'D': return 0x07; case 'E': return 0x08; case 'F': return 0x09;
    case 'G': return 0x0A; case 'H': return 0x0B; case 'I': return 0x0C;
    case 'J': return 0x0D; case 'K': return 0x0E; case 'L': return 0x0F;
    case 'M': return 0x10; case 'N': return 0x11; case 'O': return 0x12;
    case 'P': return 0x13; case 'Q': return 0x14; case 'R': return 0x15;
    case 'S': return 0x16; case 'T': return 0x17; case 'U': return 0x18;
    case 'V': return 0x19; case 'W': return 0x1A; case 'X': return 0x1B;
    case 'Y': return 0x1C; case 'Z': return 0x1D;
    case '1': return 0x1E; case '2': return 0x1F; case '3': return 0x20;
    case '4': return 0x21; case '5': return 0x22; case '6': return 0x23;
    case '7': return 0x24; case '8': return 0x25; case '9': return 0x26;
    case '0': return 0x27;
    case VK_RETURN: return 0x28; case VK_ESCAPE: return 0x29;
    case VK_BACK: return 0x2A; case VK_TAB: return 0x2B;
    case VK_SPACE: return 0x2C;
    case VK_OEM_MINUS: return 0x2D; case VK_OEM_PLUS: return 0x2E;
    case VK_F1: return 0x3A; case VK_F2: return 0x3B; case VK_F3: return 0x3C;
    case VK_F4: return 0x3D; case VK_F5: return 0x3E; case VK_F6: return 0x3F;
    case VK_F7: return 0x40; case VK_F8: return 0x41; case VK_F9: return 0x42;
    case VK_F10: return 0x43; case VK_F11: return 0x44; case VK_F12: return 0x45;
    case VK_INSERT: return 0x49; case VK_HOME: return 0x4A;
    case VK_PRIOR: return 0x4B; case VK_DELETE: return 0x4C;
    case VK_END: return 0x4D; case VK_NEXT: return 0x4E;
    case VK_RIGHT: return 0x4F; case VK_LEFT: return 0x50;
    case VK_DOWN: return 0x51; case VK_UP: return 0x52;
    case VK_LCONTROL: return 0xE0; case VK_LSHIFT: return 0xE1;
    case VK_LMENU: return 0xE2; case VK_LWIN: return 0xE3;
    case VK_RCONTROL: return 0xE4; case VK_RSHIFT: return 0xE5;
    case VK_RMENU: return 0xE6; case VK_RWIN: return 0xE7;
    default: return 0;
    }
}

uint16_t hid_to_win_vk(uint16_t hid) {
    // Reverse mapping - iterate all possible VK codes
    for (uint16_t vk = 0; vk < 256; vk++) {
        if (win_vk_to_hid(vk) == hid) return vk;
    }
    return 0;
}

static uint32_t win_mods_to_modifiers() {
    uint32_t mods = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000)   mods |= MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000)    mods |= MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mods |= MOD_GUI;
    return mods;
}

class WinInputCapture : public InputCapture {
public:
    ~WinInputCapture() override { stop(); }

    bool start(EventCallback callback) override {
        if (capturing_) return false;
        callback_ = std::move(callback);
        instance_ = this;
        capturing_ = true;

        hook_thread_ = std::thread([this]() {
            kb_hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_proc, nullptr, 0);
            mouse_hook_ = SetWindowsHookExW(WH_MOUSE_LL, mouse_proc, nullptr, 0);

            MSG msg;
            while (GetMessageW(&msg, nullptr, 0, 0) && capturing_) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            if (kb_hook_) { UnhookWindowsHookEx(kb_hook_); kb_hook_ = nullptr; }
            if (mouse_hook_) { UnhookWindowsHookEx(mouse_hook_); mouse_hook_ = nullptr; }
        });

        return true;
    }

    void stop() override {
        if (!capturing_) return;
        capturing_ = false;
        PostThreadMessage(GetThreadId(hook_thread_.native_handle()), WM_QUIT, 0, 0);
        if (hook_thread_.joinable()) hook_thread_.join();
        instance_ = nullptr;
    }

    void set_suppress(bool suppress) override { suppress_ = suppress; }
    bool is_capturing() const override { return capturing_; }

private:
    static LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION && instance_ && instance_->callback_) {
            auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

            // Scroll Lock = emergency escape hotkey (always goes back to local)
            if (kb->vkCode == VK_SCROLL && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
                if (instance_->suppress_) {
                    instance_->suppress_ = false;
                    if (instance_->escape_callback_) {
                        instance_->escape_callback_();
                    }
                    return 1; // consume the key
                }
            }

            uint16_t hid = win_vk_to_hid(static_cast<uint16_t>(kb->vkCode));
            uint32_t mods = win_mods_to_modifiers();

            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                instance_->callback_(make_message(KeyDown{hid, mods}));
            } else {
                instance_->callback_(make_message(KeyUp{hid, mods}));
            }

            if (instance_->suppress_) return 1;
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    static LRESULT CALLBACK mouse_proc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION && instance_ && instance_->callback_) {
            auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

            switch (wParam) {
            case WM_MOUSEMOVE:
                instance_->callback_(make_message(MouseMoveAbs{
                    static_cast<uint16_t>(ms->pt.x),
                    static_cast<uint16_t>(ms->pt.y)
                }));
                break;
            case WM_LBUTTONDOWN:
                instance_->callback_(make_message(MouseButton{0, 1}));
                break;
            case WM_LBUTTONUP:
                instance_->callback_(make_message(MouseButton{0, 0}));
                break;
            case WM_RBUTTONDOWN:
                instance_->callback_(make_message(MouseButton{1, 1}));
                break;
            case WM_RBUTTONUP:
                instance_->callback_(make_message(MouseButton{1, 0}));
                break;
            case WM_MBUTTONDOWN:
                instance_->callback_(make_message(MouseButton{2, 1}));
                break;
            case WM_MBUTTONUP:
                instance_->callback_(make_message(MouseButton{2, 0}));
                break;
            case WM_MOUSEWHEEL: {
                int16_t delta = static_cast<int16_t>(GET_WHEEL_DELTA_WPARAM(ms->mouseData));
                instance_->callback_(make_message(MouseScroll{0, delta}));
                break;
            }
            case WM_MOUSEHWHEEL: {
                int16_t delta = static_cast<int16_t>(GET_WHEEL_DELTA_WPARAM(ms->mouseData));
                instance_->callback_(make_message(MouseScroll{delta, 0}));
                break;
            }
            }

            if (instance_->suppress_) return 1;
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    void set_escape_callback(std::function<void()> cb) override {
        escape_callback_ = std::move(cb);
    }

    static inline WinInputCapture* instance_ = nullptr;
    HHOOK kb_hook_ = nullptr;
    HHOOK mouse_hook_ = nullptr;
    std::thread hook_thread_;
    EventCallback callback_;
    std::function<void()> escape_callback_;
    std::atomic<bool> capturing_{false};
    std::atomic<bool> suppress_{false};
};

std::unique_ptr<InputCapture> create_input_capture() {
    return std::make_unique<WinInputCapture>();
}

} // namespace smouse

#endif // _WIN32
