#pragma once

#include <memory>
#include <string>

#include "platform/types.h"

namespace nebula::window {

struct NebulaWindowImpl;

using BrowserLayout = nebula::platform::BrowserLayout;

class WindowDelegate {
public:
    virtual ~WindowDelegate() = default;
    virtual void OnWindowCreated() = 0;
    virtual void OnWindowResized(const BrowserLayout& layout) = 0;
    virtual void OnWindowCloseRequested() = 0;
};

class NebulaWindow {
public:
    explicit NebulaWindow(WindowDelegate* delegate);
    ~NebulaWindow();

    bool Create(const nebula::platform::AppStartup& startup);
    nebula::platform::NativeWindow native_handle() const;
    BrowserLayout CurrentLayout(bool show_chrome = true) const;

    void ResizeChild(nebula::platform::NativeWindow child, const nebula::platform::Rect& rect) const;
    void Minimize();
    void ToggleMaximize();
    void SetFullscreen(bool fullscreen);
    void Close();
    void BeginDrag();
    void SetTitle(const std::string& title);
    void EnableFrameHitTest(nebula::platform::NativeWindow child) const;

private:
    std::unique_ptr<NebulaWindowImpl> impl_;
};

}  // namespace nebula::window
