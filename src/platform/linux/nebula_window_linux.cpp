#include "window/nebula_window.h"

#include <memory>

namespace nebula::window {

struct nebula::window::NebulaWindowImpl {
    WindowDelegate* delegate = nullptr;
};

NebulaWindow::NebulaWindow(WindowDelegate* delegate)
    : impl_(std::make_unique<NebulaWindowImpl>()) {
    impl_->delegate = delegate;
}

NebulaWindow::~NebulaWindow() = default;

bool NebulaWindow::Create(const platform::AppStartup& startup) {
    UNREFERENCED_PARAMETER(startup);
    return false;
}

platform::NativeWindow NebulaWindow::native_handle() const {
    return nullptr;
}

BrowserLayout NebulaWindow::CurrentLayout(bool show_chrome) const {
    UNREFERENCED_PARAMETER(show_chrome);
    return {};
}

void NebulaWindow::ResizeChild(platform::NativeWindow child, const platform::Rect& rect) const {
    UNREFERENCED_PARAMETER(child);
    UNREFERENCED_PARAMETER(rect);
}

void NebulaWindow::Minimize() {}
void NebulaWindow::ToggleMaximize() {}
void NebulaWindow::SetFullscreen(bool fullscreen) { UNREFERENCED_PARAMETER(fullscreen); }
void NebulaWindow::Close() {}
void NebulaWindow::BeginDrag() {}
void NebulaWindow::SetTitle(const std::string& title) { UNREFERENCED_PARAMETER(title); }
void NebulaWindow::EnableFrameHitTest(platform::NativeWindow child) const { UNREFERENCED_PARAMETER(child); }

}  // namespace nebula::window
