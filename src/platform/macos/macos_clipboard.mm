#import "core/clipboard.h"
#import <AppKit/AppKit.h>
#import <thread>
#import <atomic>

namespace smouse {

class MacOSClipboardMonitor : public ClipboardMonitor {
public:
    ~MacOSClipboardMonitor() override {
        stop();
    }

    void start(ChangeCallback callback) override {
        callback_ = std::move(callback);
        running_ = true;
        last_change_count_ = [[NSPasteboard generalPasteboard] changeCount];

        poll_thread_ = std::thread([this]() {
            while (running_) {
                @autoreleasepool {
                    NSPasteboard* pb = [NSPasteboard generalPasteboard];
                    NSInteger current = [pb changeCount];

                    if (current != last_change_count_) {
                        last_change_count_ = current;
                        auto content = read_clipboard(pb);
                        if (content && callback_) {
                            callback_(content->first, std::move(content->second));
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        });
    }

    void stop() override {
        running_ = false;
        if (poll_thread_.joinable()) {
            poll_thread_.join();
        }
    }

    std::optional<std::pair<ClipboardFormat, std::vector<uint8_t>>> get_content() override {
        @autoreleasepool {
            return read_clipboard([NSPasteboard generalPasteboard]);
        }
    }

    void set_content(ClipboardFormat format, const std::vector<uint8_t>& data) override {
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            [pb clearContents];

            switch (format) {
            case ClipboardFormat::TEXT: {
                NSString* str = [[NSString alloc]
                    initWithBytes:data.data()
                    length:data.size()
                    encoding:NSUTF8StringEncoding];
                if (str) {
                    [pb setString:str forType:NSPasteboardTypeString];
                }
                break;
            }
            case ClipboardFormat::HTML: {
                NSString* html = [[NSString alloc]
                    initWithBytes:data.data()
                    length:data.size()
                    encoding:NSUTF8StringEncoding];
                if (html) {
                    [pb setString:html forType:NSPasteboardTypeHTML];
                }
                break;
            }
            case ClipboardFormat::IMAGE_PNG: {
                NSData* imgData = [NSData dataWithBytes:data.data() length:data.size()];
                [pb setData:imgData forType:NSPasteboardTypePNG];
                break;
            }
            }

            // Update change count to avoid re-triggering our own change
            last_change_count_ = [pb changeCount];
        }
    }

private:
    std::optional<std::pair<ClipboardFormat, std::vector<uint8_t>>>
    read_clipboard(NSPasteboard* pb) {
        // Try PNG image first
        NSData* pngData = [pb dataForType:NSPasteboardTypePNG];
        if (pngData) {
            const uint8_t* bytes = static_cast<const uint8_t*>([pngData bytes]);
            return std::make_pair(
                ClipboardFormat::IMAGE_PNG,
                std::vector<uint8_t>(bytes, bytes + [pngData length])
            );
        }

        // Try TIFF (common on macOS for screenshots) -> convert to PNG
        NSData* tiffData = [pb dataForType:NSPasteboardTypeTIFF];
        if (tiffData) {
            NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData:tiffData];
            if (rep) {
                NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG
                                              properties:@{}];
                if (png) {
                    const uint8_t* bytes = static_cast<const uint8_t*>([png bytes]);
                    return std::make_pair(
                        ClipboardFormat::IMAGE_PNG,
                        std::vector<uint8_t>(bytes, bytes + [png length])
                    );
                }
            }
        }

        // Try HTML
        NSString* html = [pb stringForType:NSPasteboardTypeHTML];
        if (html) {
            const char* utf8 = [html UTF8String];
            return std::make_pair(
                ClipboardFormat::HTML,
                std::vector<uint8_t>(utf8, utf8 + strlen(utf8))
            );
        }

        // Try plain text
        NSString* text = [pb stringForType:NSPasteboardTypeString];
        if (text) {
            const char* utf8 = [text UTF8String];
            return std::make_pair(
                ClipboardFormat::TEXT,
                std::vector<uint8_t>(utf8, utf8 + strlen(utf8))
            );
        }

        return std::nullopt;
    }

    ChangeCallback callback_;
    std::thread poll_thread_;
    std::atomic<bool> running_{false};
    NSInteger last_change_count_ = 0;
};

std::unique_ptr<ClipboardMonitor> create_clipboard_monitor() {
    return std::make_unique<MacOSClipboardMonitor>();
}

} // namespace smouse
