#include "window/nebula_window.h"

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace nebula::window {
struct NebulaWindowImpl;
}  // namespace nebula::window

@interface NebulaContentView : NSView
@end

@implementation NebulaContentView
- (BOOL)isFlipped {
    return YES;
}
@end

@interface NebulaWindowDelegate : NSObject <NSWindowDelegate> {
@private
    nebula::window::NebulaWindowImpl* owner_;
}
- (instancetype)initWithOwner:(nebula::window::NebulaWindowImpl*)owner;
@end

namespace nebula::window {
namespace {

constexpr CGFloat kChromeHeight = 104.0;

NSRect ToNativeRect(const platform::Rect& rect) {
    return NSMakeRect(rect.x, rect.y, std::max(0, rect.width), std::max(0, rect.height));
}

}  // namespace

struct NebulaWindowImpl {
    WindowDelegate* delegate = nullptr;
    NSWindow* window = nil;
    NebulaContentView* content_view = nil;
    NebulaWindowDelegate* window_delegate = nil;
    bool fullscreen = false;

    BrowserLayout CurrentLayout(bool show_chrome) const {
        const NSRect bounds = content_view ? [content_view bounds] : NSZeroRect;
        const int width = std::max(0, static_cast<int>(std::round(NSWidth(bounds))));
        const int height = std::max(0, static_cast<int>(std::round(NSHeight(bounds))));
        const int chrome_height = show_chrome ? std::min(height, static_cast<int>(kChromeHeight)) : 0;

        BrowserLayout layout;
        layout.chrome = show_chrome ? platform::Rect{0, 0, width, chrome_height} : platform::Rect{};
        layout.content = {0, chrome_height, width, std::max(0, height - chrome_height)};
        return layout;
    }

    void NotifyCreated() const {
        if (delegate) {
            delegate->OnWindowCreated();
        }
    }

    void NotifyResized() const {
        if (delegate) {
            delegate->OnWindowResized(CurrentLayout(true));
        }
    }

    void NotifyCloseRequested() const {
        if (delegate) {
            delegate->OnWindowCloseRequested();
        }
    }
};

}  // namespace nebula::window

@implementation NebulaWindowDelegate
- (instancetype)initWithOwner:(nebula::window::NebulaWindowImpl*)owner {
    self = [super init];
    if (self) {
        owner_ = owner;
    }
    return self;
}

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (owner_) {
        owner_->NotifyResized();
    }
}

- (BOOL)windowShouldClose:(id)sender {
    (void)sender;
    if (owner_) {
        owner_->NotifyCloseRequested();
    }
    return NO;
}
@end

namespace nebula::window {

NebulaWindow::NebulaWindow(WindowDelegate* delegate)
    : impl_(std::make_unique<NebulaWindowImpl>()) {
    impl_->delegate = delegate;
}

NebulaWindow::~NebulaWindow() = default;

bool NebulaWindow::Create(const platform::AppStartup& startup) {
    (void)startup;

    @autoreleasepool {
        [NSApplication sharedApplication];

        const NSRect visible_frame = [[NSScreen mainScreen] visibleFrame];
        const CGFloat width = std::min<CGFloat>(1400.0, NSWidth(visible_frame));
        const CGFloat height = std::min<CGFloat>(900.0, NSHeight(visible_frame));
        const CGFloat x = NSMinX(visible_frame) + (NSWidth(visible_frame) - width) / 2.0;
        const CGFloat y = NSMinY(visible_frame) + (NSHeight(visible_frame) - height) / 2.0;

        impl_->content_view = [[NebulaContentView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];
        impl_->window_delegate = [[NebulaWindowDelegate alloc] initWithOwner:impl_.get()];
        impl_->window = [[NSWindow alloc] initWithContentRect:NSMakeRect(x, y, width, height)
                                                    styleMask:NSWindowStyleMaskTitled |
                                                              NSWindowStyleMaskClosable |
                                                              NSWindowStyleMaskMiniaturizable |
                                                              NSWindowStyleMaskResizable |
                                                              NSWindowStyleMaskFullSizeContentView
                                                      backing:NSBackingStoreBuffered
                                                        defer:NO];
        if (!impl_->window) {
            return false;
        }

        [impl_->window setTitle:@"Nebula Browser"];
        [impl_->window setTitleVisibility:NSWindowTitleHidden];
        [impl_->window setTitlebarAppearsTransparent:YES];
        [impl_->window setContentView:impl_->content_view];
        [impl_->window setDelegate:impl_->window_delegate];
        [impl_->window center];
        [impl_->window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        impl_->NotifyCreated();
        return true;
    }
}

platform::NativeWindow NebulaWindow::native_handle() const {
    return (__bridge void*)impl_->content_view;
}

BrowserLayout NebulaWindow::CurrentLayout(bool show_chrome) const {
    return impl_->CurrentLayout(show_chrome);
}

void NebulaWindow::ResizeChild(platform::NativeWindow child, const platform::Rect& rect) const {
    NSView* view = (__bridge NSView*)child;
    if (!view) {
        return;
    }

    [view setFrame:ToNativeRect(rect)];
}

void NebulaWindow::Minimize() {
    [impl_->window miniaturize:nil];
}

void NebulaWindow::ToggleMaximize() {
    if (impl_->window && !impl_->fullscreen) {
        [impl_->window zoom:nil];
    }
}

void NebulaWindow::SetFullscreen(bool fullscreen) {
    if (!impl_->window || impl_->fullscreen == fullscreen) {
        return;
    }

    impl_->fullscreen = fullscreen;
    [impl_->window toggleFullScreen:nil];
}

void NebulaWindow::Close() {
    if (impl_->delegate) {
        impl_->delegate->OnWindowCloseRequested();
    }
}

void NebulaWindow::BeginDrag() {
    if (!impl_->window) {
        return;
    }

    NSEvent* event = [NSApp currentEvent];
    if (event) {
        [impl_->window performWindowDragWithEvent:event];
    }
}

void NebulaWindow::SetTitle(const std::string& title) {
    if (!impl_->window) {
        return;
    }

    NSString* native_title = title.empty()
        ? @"Nebula Browser"
        : [[NSString alloc] initWithUTF8String:title.c_str()];
    [impl_->window setTitle:native_title ?: @"Nebula Browser"];
}

void NebulaWindow::EnableFrameHitTest(platform::NativeWindow child) const {
    (void)child;
}

}  // namespace nebula::window
