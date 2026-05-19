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
    NEBULA_UNUSED(startup);
    return false;
}

platform::NativeWindow NebulaWindow::native_handle() const {
    return nullptr;
}

BrowserLayout NebulaWindow::CurrentLayout(bool show_chrome) const {
    NEBULA_UNUSED(show_chrome);
    return {};
}

void NebulaWindow::ResizeChild(platform::NativeWindow child, const platform::Rect& rect) const {
    NEBULA_UNUSED(child);
    NEBULA_UNUSED(rect);
}

void NebulaWindow::Minimize() {}
void NebulaWindow::ToggleMaximize() {}
void NebulaWindow::SetFullscreen(bool fullscreen) { NEBULA_UNUSED(fullscreen); }
void NebulaWindow::Close() {}
void NebulaWindow::BeginDrag() {}
void NebulaWindow::SetTitle(const std::string& title) { NEBULA_UNUSED(title); }
void NebulaWindow::EnableFrameHitTest(platform::NativeWindow child) const { NEBULA_UNUSED(child); }

}  // namespace nebula::window
