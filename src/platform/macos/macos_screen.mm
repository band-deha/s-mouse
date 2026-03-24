#import "../platform.h"
#import <ApplicationServices/ApplicationServices.h>

namespace smouse {

class MacOSScreenQuery : public ScreenQuery {
public:
    std::vector<DisplayInfo> get_displays() override {
        std::vector<DisplayInfo> displays;

        CGDirectDisplayID displayIds[16];
        uint32_t count = 0;
        CGGetActiveDisplayList(16, displayIds, &count);

        CGDirectDisplayID mainDisplay = CGMainDisplayID();

        for (uint32_t i = 0; i < count; i++) {
            CGRect bounds = CGDisplayBounds(displayIds[i]);
            DisplayInfo info;
            info.x = static_cast<int32_t>(bounds.origin.x);
            info.y = static_cast<int32_t>(bounds.origin.y);
            info.width = static_cast<uint32_t>(bounds.size.width);
            info.height = static_cast<uint32_t>(bounds.size.height);
            info.is_primary = (displayIds[i] == mainDisplay);
            displays.push_back(info);
        }

        return displays;
    }

    std::pair<int32_t, int32_t> get_cursor_position() override {
        CGEventRef event = CGEventCreate(nullptr);
        CGPoint pos = CGEventGetLocation(event);
        CFRelease(event);
        return {static_cast<int32_t>(pos.x), static_cast<int32_t>(pos.y)};
    }
};

std::unique_ptr<ScreenQuery> create_screen_query() {
    return std::make_unique<MacOSScreenQuery>();
}

} // namespace smouse
