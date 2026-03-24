#ifdef _WIN32

#include "core/clipboard.h"
#include <windows.h>
#include <atomic>
#include <thread>
#include <cstring>

namespace smouse {

class WinClipboardMonitor : public ClipboardMonitor {
public:
    ~WinClipboardMonitor() override { stop(); }

    void start(ChangeCallback callback) override {
        callback_ = std::move(callback);
        running_ = true;

        monitor_thread_ = std::thread([this]() {
            // Create a message-only window for clipboard notifications
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = wnd_proc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = L"SMouse_ClipboardMonitor";
            RegisterClassExW(&wc);

            hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"",
                0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, this);

            if (hwnd_) {
                AddClipboardFormatListener(hwnd_);

                MSG msg;
                while (GetMessageW(&msg, nullptr, 0, 0) && running_) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }

                RemoveClipboardFormatListener(hwnd_);
                DestroyWindow(hwnd_);
                hwnd_ = nullptr;
            }
        });
    }

    void stop() override {
        running_ = false;
        if (hwnd_) PostMessageW(hwnd_, WM_QUIT, 0, 0);
        if (monitor_thread_.joinable()) monitor_thread_.join();
    }

    std::optional<std::pair<ClipboardFormat, std::vector<uint8_t>>> get_content() override {
        if (!OpenClipboard(nullptr)) return std::nullopt;

        std::optional<std::pair<ClipboardFormat, std::vector<uint8_t>>> result;

        // Try PNG
        UINT png_format = RegisterClipboardFormatW(L"PNG");
        HANDLE h = GetClipboardData(png_format);
        if (h) {
            size_t size = GlobalSize(h);
            auto* data = static_cast<uint8_t*>(GlobalLock(h));
            if (data) {
                result = std::make_pair(
                    ClipboardFormat::IMAGE_PNG,
                    std::vector<uint8_t>(data, data + size));
                GlobalUnlock(h);
            }
        }

        // Try text
        if (!result) {
            h = GetClipboardData(CF_UNICODETEXT);
            if (h) {
                auto* text = static_cast<wchar_t*>(GlobalLock(h));
                if (text) {
                    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
                    std::vector<uint8_t> utf8(utf8_len);
                    WideCharToMultiByte(CP_UTF8, 0, text, -1, reinterpret_cast<char*>(utf8.data()), utf8_len, nullptr, nullptr);
                    if (!utf8.empty() && utf8.back() == 0) utf8.pop_back();
                    result = std::make_pair(ClipboardFormat::TEXT, std::move(utf8));
                    GlobalUnlock(h);
                }
            }
        }

        CloseClipboard();
        return result;
    }

    void set_content(ClipboardFormat format, const std::vector<uint8_t>& data) override {
        if (!OpenClipboard(nullptr)) return;
        EmptyClipboard();

        switch (format) {
        case ClipboardFormat::TEXT: {
            int wlen = MultiByteToWideChar(CP_UTF8, 0,
                reinterpret_cast<const char*>(data.data()),
                static_cast<int>(data.size()), nullptr, 0);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(wchar_t));
            if (hMem) {
                auto* dest = static_cast<wchar_t*>(GlobalLock(hMem));
                MultiByteToWideChar(CP_UTF8, 0,
                    reinterpret_cast<const char*>(data.data()),
                    static_cast<int>(data.size()), dest, wlen);
                dest[wlen] = 0;
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
            break;
        }
        case ClipboardFormat::IMAGE_PNG: {
            UINT png_format = RegisterClipboardFormatW(L"PNG");
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, data.size());
            if (hMem) {
                auto* dest = GlobalLock(hMem);
                std::memcpy(dest, data.data(), data.size());
                GlobalUnlock(hMem);
                SetClipboardData(png_format, hMem);
            }
            break;
        }
        case ClipboardFormat::HTML:
            // TODO: Implement HTML clipboard format (CF_HTML)
            break;
        }

        CloseClipboard();
    }

private:
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_CREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return 0;
        }

        auto* self = reinterpret_cast<WinClipboardMonitor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if (msg == WM_CLIPBOARDUPDATE && self && self->callback_) {
            auto content = self->get_content();
            if (content) {
                self->callback_(content->first, std::move(content->second));
            }
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    ChangeCallback callback_;
    std::thread monitor_thread_;
    std::atomic<bool> running_{false};
    HWND hwnd_ = nullptr;
};

std::unique_ptr<ClipboardMonitor> create_clipboard_monitor() {
    return std::make_unique<WinClipboardMonitor>();
}

} // namespace smouse

#endif // _WIN32
