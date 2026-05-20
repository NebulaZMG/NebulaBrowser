#include "window/nebula_window.h"

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <string_view>

namespace nebula::window {
namespace {

constexpr wchar_t kWindowClassName[] = L"NebulaBrowserWindow";
constexpr wchar_t kWindowTitle[] = L"Nebula Browser";
constexpr wchar_t kChildFrameHitTestOldProcProp[] = L"NebulaChildFrameHitTestOldProc";
constexpr wchar_t kChildFrameHitTestParentProp[] = L"NebulaChildFrameHitTestParent";
constexpr ULONG_PTR kOpenTargetCopyDataId = 0x4E42554CUL;
constexpr int kTitleRowHeightDip = 42;
constexpr int kWindowControlWidthDip = 46;
constexpr int kWindowControlCount = 3;
constexpr COLORREF kNoWindowBorderColor = 0xFFFFFFFE;

RECT GetWorkArea() {
    RECT work_area = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    return work_area;
}

RECT GetMonitorWorkArea(HWND hwnd) {
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);

    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor && GetMonitorInfoW(monitor, &monitor_info)) {
        return monitor_info.rcWork;
    }

    return GetWorkArea();
}

RECT GetMonitorArea(HWND hwnd) {
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);

    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor && GetMonitorInfoW(monitor, &monitor_info)) {
        return monitor_info.rcMonitor;
    }

    return GetWorkArea();
}

bool IsResizeHit(LRESULT hit) {
    return hit == HTLEFT || hit == HTRIGHT || hit == HTTOP || hit == HTBOTTOM ||
           hit == HTTOPLEFT || hit == HTTOPRIGHT || hit == HTBOTTOMLEFT || hit == HTBOTTOMRIGHT;
}

HCURSOR CursorForResizeHit(LRESULT hit) {
    switch (hit) {
        case HTLEFT:
        case HTRIGHT:
            return LoadCursor(nullptr, IDC_SIZEWE);
        case HTTOP:
        case HTBOTTOM:
            return LoadCursor(nullptr, IDC_SIZENS);
        case HTTOPLEFT:
        case HTBOTTOMRIGHT:
            return LoadCursor(nullptr, IDC_SIZENWSE);
        case HTTOPRIGHT:
        case HTBOTTOMLEFT:
            return LoadCursor(nullptr, IDC_SIZENESW);
        default:
            return nullptr;
    }
}

bool SetResizeCursor(LRESULT hit) {
    HCURSOR cursor = CursorForResizeHit(hit);
    if (!cursor) {
        return false;
    }

    SetCursor(cursor);
    return true;
}

void ApplyWindowFrameStyle(HWND hwnd) {
    const BOOL dark_mode = TRUE;
    const DWM_WINDOW_CORNER_PREFERENCE corner_preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(
        hwnd,
        DWMWA_USE_IMMERSIVE_DARK_MODE,
        &dark_mode,
        sizeof(dark_mode));

    DwmSetWindowAttribute(
        hwnd,
        DWMWA_WINDOW_CORNER_PREFERENCE,
        &corner_preference,
        sizeof(corner_preference));

    DwmSetWindowAttribute(
        hwnd,
        DWMWA_BORDER_COLOR,
        &kNoWindowBorderColor,
        sizeof(kNoWindowBorderColor));
}

platform::Rect ToPlatformRect(const RECT& rect) {
    return {
        rect.left,
        rect.top,
        std::max(0L, rect.right - rect.left),
        std::max(0L, rect.bottom - rect.top),
    };
}

RECT ToNativeRect(const platform::Rect& rect) {
    return {
        rect.x,
        rect.y,
        rect.x + rect.width,
        rect.y + rect.height,
    };
}

std::string WideToUtf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(size, '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        size,
        nullptr,
        nullptr);
    return result;
}

}  // namespace

struct nebula::window::NebulaWindowImpl {
    WindowDelegate* delegate = nullptr;
    HINSTANCE instance = nullptr;
    HWND hwnd = nullptr;
    bool fullscreen = false;
    LONG_PTR restore_style = 0;
    LONG_PTR restore_ex_style = 0;
    WINDOWPLACEMENT restore_placement = {sizeof(WINDOWPLACEMENT)};
    UINT dpi = 96;
    int resize_border_dip = 8;
    int chrome_height_dip = 104;

    int ScaleForDpi(int value) const {
        return MulDiv(value, static_cast<int>(dpi), 96);
    }

    void UpdateDpi() {
        if (hwnd) {
            dpi = GetDpiForWindow(hwnd);
        }
    }

    void NotifyResize() {
        if (delegate) {
            delegate->OnWindowResized(CurrentLayout(true));
        }
    }

    BrowserLayout CurrentLayout(bool show_chrome) const {
        RECT client = {};
        if (hwnd) {
            GetClientRect(hwnd, &client);
        }

        BrowserLayout layout;
        layout.chrome = show_chrome
            ? ToPlatformRect(RECT{
                  0,
                  0,
                  client.right,
                  std::min<LONG>(ScaleForDpi(chrome_height_dip), client.bottom)})
            : platform::Rect{};
        layout.content = ToPlatformRect(
            RECT{0, layout.chrome.y + layout.chrome.height, client.right, client.bottom});
        return layout;
    }

    void EnableFrameHitTestForWindow(HWND child) const;
    LRESULT HitTest(LPARAM lparam) const;
    LRESULT HitTestPoint(POINT point) const;
    LRESULT WndProc(UINT message, WPARAM wparam, LPARAM lparam);
    void RegisterClass(HINSTANCE instance_handle);

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK ChildFrameWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static BOOL CALLBACK EnableFrameHitTestForDescendant(HWND hwnd, LPARAM lparam);
};

LRESULT CALLBACK nebula::window::NebulaWindowImpl::StaticWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    NebulaWindowImpl* self = nullptr;

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<NebulaWindowImpl*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd = hwnd;
    } else {
        self = reinterpret_cast<NebulaWindowImpl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    return self ? self->WndProc(message, wparam, lparam)
                : DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK nebula::window::NebulaWindowImpl::ChildFrameWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto old_proc = reinterpret_cast<WNDPROC>(GetPropW(hwnd, kChildFrameHitTestOldProcProp));

    if (message == WM_NCHITTEST) {
        const auto parent = reinterpret_cast<HWND>(GetPropW(hwnd, kChildFrameHitTestParentProp));
        auto* self = parent ? reinterpret_cast<NebulaWindowImpl*>(GetWindowLongPtrW(parent, GWLP_USERDATA)) : nullptr;
        if (self) {
            const LRESULT hit = self->HitTest(lparam);
            if (IsResizeHit(hit)) {
                return hit;
            }
        }
    }

    if (message == WM_SETCURSOR) {
        const auto parent = reinterpret_cast<HWND>(GetPropW(hwnd, kChildFrameHitTestParentProp));
        auto* self = parent ? reinterpret_cast<NebulaWindowImpl*>(GetWindowLongPtrW(parent, GWLP_USERDATA)) : nullptr;
        POINT point = {};
        if (self && GetCursorPos(&point) && SetResizeCursor(self->HitTestPoint(point))) {
            return TRUE;
        }
    }

    if (message == WM_MOUSEMOVE || message == WM_NCMOUSEMOVE) {
        const auto parent = reinterpret_cast<HWND>(GetPropW(hwnd, kChildFrameHitTestParentProp));
        auto* self = parent ? reinterpret_cast<NebulaWindowImpl*>(GetWindowLongPtrW(parent, GWLP_USERDATA)) : nullptr;
        POINT point = {};
        if (self && GetCursorPos(&point) && SetResizeCursor(self->HitTestPoint(point))) {
            return 0;
        }
    }

    if (message == WM_NCLBUTTONDOWN && IsResizeHit(static_cast<LRESULT>(wparam))) {
        const auto parent = reinterpret_cast<HWND>(GetPropW(hwnd, kChildFrameHitTestParentProp));
        if (parent) {
            ReleaseCapture();
            SendMessageW(parent, WM_NCLBUTTONDOWN, wparam, lparam);
            return 0;
        }
    }

    if (message == WM_NCDESTROY) {
        RemovePropW(hwnd, kChildFrameHitTestParentProp);
        RemovePropW(hwnd, kChildFrameHitTestOldProcProp);
        if (old_proc) {
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(old_proc));
        }
    }

    return old_proc ? CallWindowProcW(old_proc, hwnd, message, wparam, lparam)
                    : DefWindowProcW(hwnd, message, wparam, lparam);
}

BOOL CALLBACK nebula::window::NebulaWindowImpl::EnableFrameHitTestForDescendant(HWND hwnd, LPARAM lparam) {
    const auto* self = reinterpret_cast<const NebulaWindowImpl*>(lparam);
    if (self) {
        self->EnableFrameHitTestForWindow(hwnd);
    }
    return TRUE;
}

void nebula::window::NebulaWindowImpl::EnableFrameHitTestForWindow(HWND child) const {
    if (!child || GetPropW(child, kChildFrameHitTestOldProcProp)) {
        return;
    }

    SetPropW(child, kChildFrameHitTestParentProp, hwnd);
    const auto old_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(child, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&NebulaWindowImpl::ChildFrameWndProc)));
    if (old_proc) {
        SetPropW(child, kChildFrameHitTestOldProcProp, reinterpret_cast<HANDLE>(old_proc));
    } else {
        RemovePropW(child, kChildFrameHitTestParentProp);
    }
}

LRESULT nebula::window::NebulaWindowImpl::WndProc(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            UpdateDpi();
            if (delegate) {
                delegate->OnWindowCreated();
            }
            return 0;

        case WM_NCCALCSIZE:
            if (wparam == TRUE) {
                return 0;
            }
            break;

        case WM_NCACTIVATE:
            ApplyWindowFrameStyle(hwnd);
            return TRUE;

        case WM_ERASEBKGND:
            return 1;

        case WM_NCHITTEST:
            return HitTest(lparam);

        case WM_SETCURSOR: {
            POINT point = {};
            if (GetCursorPos(&point) && SetResizeCursor(HitTestPoint(point))) {
                return TRUE;
            }
            break;
        }

        case WM_MOUSEMOVE:
        case WM_NCMOUSEMOVE: {
            POINT point = {};
            if (GetCursorPos(&point) && SetResizeCursor(HitTestPoint(point))) {
                return 0;
            }
            break;
        }

        case WM_SIZE:
            NotifyResize();
            return 0;

        case WM_DPICHANGED: {
            dpi = HIWORD(wparam);
            const auto* suggested_rect = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(
                hwnd,
                nullptr,
                suggested_rect->left,
                suggested_rect->top,
                suggested_rect->right - suggested_rect->left,
                suggested_rect->bottom - suggested_rect->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            NotifyResize();
            return 0;
        }

        case WM_GETMINMAXINFO: {
            const RECT work_area = GetMonitorWorkArea(hwnd);
            const RECT monitor_area = GetMonitorArea(hwnd);

            auto* minmax = reinterpret_cast<MINMAXINFO*>(lparam);
            minmax->ptMaxPosition.x = work_area.left - monitor_area.left;
            minmax->ptMaxPosition.y = work_area.top - monitor_area.top;
            minmax->ptMaxSize.x = work_area.right - work_area.left;
            minmax->ptMaxSize.y = work_area.bottom - work_area.top;
            return 0;
        }

        case WM_CLOSE:
            if (delegate) {
                delegate->OnWindowCloseRequested();
                return 0;
            }
            break;

        case WM_COPYDATA: {
            const auto* copy_data = reinterpret_cast<const COPYDATASTRUCT*>(lparam);
            if (copy_data && copy_data->dwData == kOpenTargetCopyDataId &&
                copy_data->lpData && copy_data->cbData >= sizeof(wchar_t)) {
                const auto* text = static_cast<const wchar_t*>(copy_data->lpData);
                const size_t char_count = (copy_data->cbData / sizeof(wchar_t)) - 1;
                if (delegate) {
                    delegate->OnExternalOpenRequested(WideToUtf8(std::wstring_view(text, char_count)));
                }
                return TRUE;
            }
            break;
        }

        case WM_DESTROY:
            hwnd = nullptr;
            return 0;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void nebula::window::NebulaWindowImpl::RegisterClass(HINSTANCE instance_handle) {
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = StaticWndProc;
    window_class.hInstance = instance_handle;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClassName;

    RegisterClassExW(&window_class);
}

LRESULT nebula::window::NebulaWindowImpl::HitTest(LPARAM lparam) const {
    if (!hwnd) {
        return HTNOWHERE;
    }

    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    return HitTestPoint(point);
}

LRESULT nebula::window::NebulaWindowImpl::HitTestPoint(POINT point) const {
    if (!hwnd) {
        return HTNOWHERE;
    }

    RECT window = {};
    GetWindowRect(hwnd, &window);

    if (fullscreen || IsZoomed(hwnd)) {
        return HTCLIENT;
    }

    const int resize_border = ScaleForDpi(resize_border_dip);
    const bool left = point.x >= window.left && point.x < window.left + resize_border;
    const bool right = point.x < window.right && point.x >= window.right - resize_border;
    const bool top = point.y >= window.top && point.y < window.top + resize_border;
    const bool bottom = point.y < window.bottom && point.y >= window.bottom - resize_border;

    if (top && left) {
        return HTTOPLEFT;
    }
    if (top && right) {
        return HTTOPRIGHT;
    }
    if (bottom && left) {
        return HTBOTTOMLEFT;
    }
    if (bottom && right) {
        return HTBOTTOMRIGHT;
    }
    if (left) {
        return HTLEFT;
    }
    if (right) {
        return HTRIGHT;
    }
    if (top) {
        return HTTOP;
    }
    if (bottom) {
        return HTBOTTOM;
    }

    const int controls_width = ScaleForDpi(kWindowControlWidthDip * kWindowControlCount);
    const int controls_height = ScaleForDpi(kTitleRowHeightDip);
    const bool window_controls = point.x >= window.right - controls_width && point.x < window.right &&
                                 point.y >= window.top && point.y < window.top + controls_height;
    if (window_controls) {
        return HTCLIENT;
    }

    return HTCLIENT;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

}  // namespace

namespace nebula::window {

NebulaWindow::NebulaWindow(WindowDelegate* delegate)
    : impl_(std::make_unique<NebulaWindowImpl>()) {
    impl_->delegate = delegate;
}

NebulaWindow::~NebulaWindow() = default;

bool NebulaWindow::Create(const platform::AppStartup& startup) {
    impl_->instance = static_cast<HINSTANCE>(startup.instance);
    impl_->RegisterClass(impl_->instance);

    const RECT work_area = GetWorkArea();
    impl_->dpi = GetDpiForSystem();
    const int width =
        std::min<LONG>(impl_->ScaleForDpi(1400), work_area.right - work_area.left);
    const int height =
        std::min<LONG>(impl_->ScaleForDpi(900), work_area.bottom - work_area.top);
    const int x = work_area.left + ((work_area.right - work_area.left) - width) / 2;
    const int y = work_area.top + ((work_area.bottom - work_area.top) - height) / 2;

    impl_->hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN,
        x,
        y,
        width,
        height,
        nullptr,
        nullptr,
        impl_->instance,
        impl_.get());

    if (!impl_->hwnd) {
        return false;
    }

    impl_->UpdateDpi();
    ApplyWindowFrameStyle(impl_->hwnd);

    const MARGINS margins = {0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(impl_->hwnd, &margins);

    ShowWindow(impl_->hwnd, startup.show_command);
    UpdateWindow(impl_->hwnd);
    return true;
}

platform::NativeWindow NebulaWindow::native_handle() const {
    return impl_->hwnd;
}

BrowserLayout NebulaWindow::CurrentLayout(bool show_chrome) const {
    return impl_->CurrentLayout(show_chrome);
}

void NebulaWindow::ResizeChild(platform::NativeWindow child, const platform::Rect& rect) const {
    const HWND hwnd = static_cast<HWND>(child);
    if (!hwnd) {
        return;
    }

    EnableFrameHitTest(child);
    const RECT native_rect = ToNativeRect(rect);
    SetWindowPos(
        hwnd,
        nullptr,
        native_rect.left,
        native_rect.top,
        std::max(0L, native_rect.right - native_rect.left),
        std::max(0L, native_rect.bottom - native_rect.top),
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void NebulaWindow::Minimize() {
    if (impl_->hwnd) {
        ShowWindow(impl_->hwnd, SW_MINIMIZE);
    }
}

void NebulaWindow::ToggleMaximize() {
    if (!impl_->hwnd || impl_->fullscreen) {
        return;
    }

    ShowWindow(impl_->hwnd, IsZoomed(impl_->hwnd) ? SW_RESTORE : SW_MAXIMIZE);
}

void NebulaWindow::SetFullscreen(bool fullscreen) {
    if (!impl_->hwnd || impl_->fullscreen == fullscreen) {
        return;
    }

    if (fullscreen) {
        impl_->restore_style = GetWindowLongPtrW(impl_->hwnd, GWL_STYLE);
        impl_->restore_ex_style = GetWindowLongPtrW(impl_->hwnd, GWL_EXSTYLE);
        impl_->restore_placement.length = sizeof(impl_->restore_placement);
        GetWindowPlacement(impl_->hwnd, &impl_->restore_placement);

        impl_->fullscreen = true;
        const RECT monitor = GetMonitorArea(impl_->hwnd);
        SetWindowLongPtrW(
            impl_->hwnd,
            GWL_STYLE,
            impl_->restore_style & ~(WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX));
        SetWindowLongPtrW(impl_->hwnd, GWL_EXSTYLE, impl_->restore_ex_style);
        SetWindowPos(
            impl_->hwnd,
            HWND_TOPMOST,
            monitor.left,
            monitor.top,
            monitor.right - monitor.left,
            monitor.bottom - monitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        impl_->fullscreen = false;
        SetWindowLongPtrW(impl_->hwnd, GWL_STYLE, impl_->restore_style);
        SetWindowLongPtrW(impl_->hwnd, GWL_EXSTYLE, impl_->restore_ex_style);
        SetWindowPlacement(impl_->hwnd, &impl_->restore_placement);
        SetWindowPos(
            impl_->hwnd,
            HWND_NOTOPMOST,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        ApplyWindowFrameStyle(impl_->hwnd);
    }

    impl_->NotifyResize();
}

void NebulaWindow::Close() {
    if (impl_->hwnd) {
        SendMessageW(impl_->hwnd, WM_CLOSE, 0, 0);
    }
}

void NebulaWindow::BeginDrag() {
    if (!impl_->hwnd) {
        return;
    }

    ReleaseCapture();
    SendMessageW(impl_->hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

void NebulaWindow::SetTitle(const std::string& title) {
    if (!impl_->hwnd) {
        return;
    }

    const std::wstring wide = title.empty() ? kWindowTitle : Utf8ToWide(title);
    SetWindowTextW(impl_->hwnd, wide.c_str());
}

void NebulaWindow::EnableFrameHitTest(platform::NativeWindow child) const {
    if (!impl_->hwnd || !child) {
        return;
    }

    impl_->EnableFrameHitTestForWindow(static_cast<HWND>(child));
    EnumChildWindows(
        static_cast<HWND>(child),
        &NebulaWindowImpl::EnableFrameHitTestForDescendant,
        reinterpret_cast<LPARAM>(impl_.get()));
}

}  // namespace nebula::window
