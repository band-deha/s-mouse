#ifdef _WIN32

#include "../platform.h"
#include <windows.h>

namespace smouse {

class WinScreenQuery : public ScreenQuery {
public:
    std::vector<DisplayInfo> get_displays() override {
        displays_.clear();
        EnumDisplayMonitors(nullptr, nullptr, monitor_enum_proc, reinterpret_cast<LPARAM>(this));
        return displays_;
    }

    std::pair<int32_t, int32_t> get_cursor_position() override {
        POINT pt;
        GetCursorPos(&pt);
        return {pt.x, pt.y};
    }

private:
    static BOOL CALLBACK monitor_enum_proc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData) {
        auto* self = reinterpret_cast<WinScreenQuery*>(dwData);

        MONITORINFO info;
        info.cbSize = sizeof(MONITORINFO);
        GetMonitorInfoW(hMonitor, &info);

        DisplayInfo di;
        di.x = info.rcMonitor.left;
        di.y = info.rcMonitor.top;
        di.width = static_cast<uint32_t>(info.rcMonitor.right - info.rcMonitor.left);
        di.height = static_cast<uint32_t>(info.rcMonitor.bottom - info.rcMonitor.top);
        di.is_primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
        self->displays_.push_back(di);

        return TRUE;
    }

    std::vector<DisplayInfo> displays_;
};

std::unique_ptr<ScreenQuery> create_screen_query() {
    return std::make_unique<WinScreenQuery>();
}

} // namespace smouse

#endif // _WIN32
