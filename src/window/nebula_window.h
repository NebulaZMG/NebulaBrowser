#pragma once

#include <windows.h>

#include <string>

namespace nebula::window {

struct BrowserLayout {
    RECT chrome = {};
    RECT content = {};
};

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

    bool Create(HINSTANCE instance, int show_command);
    HWND hwnd() const { return hwnd_; }
    BrowserLayout CurrentLayout(bool show_chrome = true) const;

    void ResizeChild(HWND child, const RECT& rect) const;
    void Minimize();
    void ToggleMaximize();
    void SetFullscreen(bool fullscreen);
    void Close();
    void BeginDrag();
    void SetTitle(const std::wstring& title);
    void EnableFrameHitTest(HWND child) const;

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK ChildFrameWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static BOOL CALLBACK EnableFrameHitTestForDescendant(HWND hwnd, LPARAM lparam);
    LRESULT WndProc(UINT message, WPARAM wparam, LPARAM lparam);

    void RegisterClass(HINSTANCE instance);
    void NotifyResize();
    void EnableFrameHitTestForWindow(HWND child) const;
    LRESULT HitTest(LPARAM lparam) const;
    LRESULT HitTestPoint(POINT point) const;
    int ScaleForDpi(int value) const;
    void UpdateDpi();

    WindowDelegate* delegate_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    bool fullscreen_ = false;
    LONG_PTR restore_style_ = 0;
    LONG_PTR restore_ex_style_ = 0;
    WINDOWPLACEMENT restore_placement_ = {sizeof(WINDOWPLACEMENT)};
    UINT dpi_ = 96;
    int resize_border_dip_ = 8;
    int chrome_height_dip_ = 104;
};

}  // namespace nebula::window
