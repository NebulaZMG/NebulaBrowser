#include "platform/browser_host.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace nebula::platform {
namespace {

NSView* AsView(NativeWindow window) {
    return (__bridge NSView*)window;
}

NSRect ToNativeRect(const Rect& rect) {
    return NSMakeRect(rect.x, rect.y, std::max(0, rect.width), std::max(0, rect.height));
}

Rect ToPlatformRect(NSRect rect) {
    return {
        static_cast<int>(std::round(NSMinX(rect))),
        static_cast<int>(std::round(NSMinY(rect))),
        std::max(0, static_cast<int>(std::round(NSWidth(rect)))),
        std::max(0, static_cast<int>(std::round(NSHeight(rect)))),
    };
}

}  // namespace

CefWindowInfo MakeChildWindowInfo(NativeWindow parent, const Rect& rect) {
    CefWindowInfo info;
    info.SetAsChild((__bridge CefWindowHandle)AsView(parent),
                    CefRect(rect.x, rect.y, rect.width, rect.height));
    info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    return info;
}

CefWindowInfo MakeDevToolsPopup(NativeWindow parent, const char* title) {
    (void)title;
    CefWindowInfo info;
    info.SetAsChild((__bridge CefWindowHandle)AsView(parent), CefRect(0, 0, 800, 600));
    info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    return info;
}

void ResizeBrowserWindow(NativeWindow browser_window, const Rect& rect) {
    NSView* view = AsView(browser_window);
    if (!view) {
        return;
    }

    [view setFrame:ToNativeRect(rect)];
}

void SetBrowserVisible(NativeWindow browser_window, bool visible) {
    NSView* view = AsView(browser_window);
    if (!view) {
        return;
    }

    [view setHidden:!visible];
    if (visible && [view superview]) {
        [[view superview] addSubview:view positioned:NSWindowAbove relativeTo:nil];
    }
}

void RaiseBrowserWindow(NativeWindow browser_window) {
    NSView* view = AsView(browser_window);
    if (view && [view superview]) {
        [[view superview] addSubview:view positioned:NSWindowAbove relativeTo:nil];
    }
}

void MoveCursorToBrowserPoint(NativeWindow browser_window, int x, int y) {
    NSView* view = AsView(browser_window);
    if (!view || ![view window]) {
        return;
    }

    const NSPoint window_point = [view convertPoint:NSMakePoint(x, y) toView:nil];
    const NSPoint screen_point = [[view window] convertPointToScreen:window_point];
    const CGFloat screen_height = NSMaxY([[NSScreen mainScreen] frame]);
    CGWarpMouseCursorPosition(CGPointMake(screen_point.x, screen_height - screen_point.y));
}

int ScaleForParentWindow(NativeWindow parent, int value) {
    (void)parent;
    return value;
}

std::pair<int, int> ParentClientSize(NativeWindow parent) {
    NSView* view = AsView(parent);
    if (!view) {
        return {0, 0};
    }

    const Rect rect = ToPlatformRect([view bounds]);
    return {rect.width, rect.height};
}

Rect MenuPopupRect(NativeWindow parent, const BrowserLayout& layout) {
    const auto client_size = ParentClientSize(parent);
    const int width = 260;
    const int height = 258;
    const int margin = 12;
    const int overlap = 2;
    const int x = std::max(0, client_size.first - width - margin);
    const int y = std::max(0, layout.chrome.y + layout.chrome.height - overlap);
    return {
        x,
        y,
        std::min(client_size.first, x + width) - x,
        std::min(client_size.second, y + height) - y,
    };
}

std::string CacheBusterToken() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return std::to_string(millis);
}

void DestroyTopLevelWindow(NativeWindow window) {
    NSView* view = AsView(window);
    NSWindow* native_window = [view window];
    if (native_window) {
        [native_window setDelegate:nil];
        [native_window close];
    }
}

}  // namespace nebula::platform
