#pragma once

#include "../platform.h"
#include <atomic>
#include <thread>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

namespace smouse {

class MacOSInputCapture : public InputCapture {
public:
    MacOSInputCapture();
    ~MacOSInputCapture() override;

    bool start(EventCallback callback) override;
    void stop() override;
    void set_suppress(bool suppress) override;
    bool is_capturing() const override;

private:
    static CGEventRef event_callback(
        CGEventTapProxy proxy, CGEventType type,
        CGEventRef event, void* refcon);

    CGEventRef handle_event(CGEventType type, CGEventRef event);

    CFMachPortRef event_tap_ = nullptr;
    CFRunLoopSourceRef run_loop_source_ = nullptr;
    CFRunLoopRef run_loop_ = nullptr;
    std::thread tap_thread_;
    EventCallback callback_;
    std::atomic<bool> capturing_{false};
    std::atomic<bool> suppress_{false};
};

} // namespace smouse
