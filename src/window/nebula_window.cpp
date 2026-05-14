#include "window/nebula_window.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>

namespace nebula::window {
namespace {

constexpr wchar_t kWindowClassName[] = L"NebulaBrowserWindow";
constexpr wchar_t kWindowTitle[] = L"Nebula Browser";
constexpr wchar_t kChildFrameHitTestOldProcProp[] = L"NebulaChildFrameHitTestOldProc";
constexpr wchar_t kChildFrameHitTestParentProp[] = L"NebulaChildFrameHitTestParent";
constexpr int kTitleRowHeightDip = 42;
constexpr int kWindowControlWidthDip = 46;
constexpr int kWindowControlCount = 3;
constexpr COLORREF kNoWindowBorderColor = 0xFFFFFFFE;

RECT GetWorkArea() {
    RECT work_area = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    return work_area;
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

}  // namespace

NebulaWindow::NebulaWindow(WindowDelegate* delegate) : delegate_(delegate) {}

NebulaWindow::~NebulaWindow() = default;

bool NebulaWindow::Create(HINSTANCE instance, int show_command) {
    instance_ = instance;
    RegisterClass(instance);

    const RECT work_area = GetWorkArea();
    dpi_ = GetDpiForSystem();
    const int width = std::min<LONG>(ScaleForDpi(1400), work_area.right - work_area.left);
    const int height = std::min<LONG>(ScaleForDpi(900), work_area.bottom - work_area.top);
    const int x = work_area.left + ((work_area.right - work_area.left) - width) / 2;
    const int y = work_area.top + ((work_area.bottom - work_area.top) - height) / 2;

    hwnd_ = CreateWindowExW(
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
        instance_,
        this);

    if (!hwnd_) {
        return false;
    }

    UpdateDpi();
    ApplyWindowFrameStyle(hwnd_);

    const MARGINS margins = {0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(hwnd_, &margins);

    ShowWindow(hwnd_, show_command);
    UpdateWindow(hwnd_);
    return true;
}

BrowserLayout NebulaWindow::CurrentLayout() const {
    RECT client = {};
    if (hwnd_) {
        GetClientRect(hwnd_, &client);
    }

    BrowserLayout layout;
    layout.chrome = {0, 0, client.right, std::min<LONG>(ScaleForDpi(chrome_height_dip_), client.bottom)};
    layout.content = {0, layout.chrome.bottom, client.right, client.bottom};
    return layout;
}

void NebulaWindow::ResizeChild(HWND child, const RECT& rect) const {
    if (!child) {
        return;
    }

    EnableFrameHitTest(child);
    SetWindowPos(
        child,
        nullptr,
        rect.left,
        rect.top,
        std::max(0L, rect.right - rect.left),
        std::max(0L, rect.bottom - rect.top),
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void NebulaWindow::Minimize() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_MINIMIZE);
    }
}

void NebulaWindow::ToggleMaximize() {
    if (!hwnd_) {
        return;
    }

    ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE);
}

void NebulaWindow::Close() {
    if (hwnd_) {
        SendMessageW(hwnd_, WM_CLOSE, 0, 0);
    }
}

void NebulaWindow::BeginDrag() {
    if (!hwnd_) {
        return;
    }

    ReleaseCapture();
    SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

void NebulaWindow::SetTitle(const std::wstring& title) {
    if (hwnd_) {
        SetWindowTextW(hwnd_, title.empty() ? kWindowTitle : title.c_str());
    }
}

void NebulaWindow::EnableFrameHitTest(HWND child) const {
    if (!hwnd_ || !child) {
        return;
    }

    EnableFrameHitTestForWindow(child);
    EnumChildWindows(child, &NebulaWindow::EnableFrameHitTestForDescendant, reinterpret_cast<LPARAM>(this));
}

LRESULT CALLBACK NebulaWindow::StaticWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    NebulaWindow* self = nullptr;

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<NebulaWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<NebulaWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    return self ? self->WndProc(message, wparam, lparam)
                : DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK NebulaWindow::ChildFrameWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto old_proc = reinterpret_cast<WNDPROC>(GetPropW(hwnd, kChildFrameHitTestOldProcProp));

    if (message == WM_NCHITTEST) {
        const auto parent = reinterpret_cast<HWND>(GetPropW(hwnd, kChildFrameHitTestParentProp));
        auto* self = parent ? reinterpret_cast<NebulaWindow*>(GetWindowLongPtrW(parent, GWLP_USERDATA)) : nullptr;
        if (self) {
            const LRESULT hit = self->HitTest(lparam);
            if (IsResizeHit(hit)) {
                return hit;
            }
        }
    }

    if (message == WM_SETCURSOR) {
        const auto parent = reinterpret_cast<HWND>(GetPropW(hwnd, kChildFrameHitTestParentProp));
        auto* self = parent ? reinterpret_cast<NebulaWindow*>(GetWindowLongPtrW(parent, GWLP_USERDATA)) : nullptr;
        POINT point = {};
        if (self && GetCursorPos(&point) && SetResizeCursor(self->HitTestPoint(point))) {
            return TRUE;
        }
    }

    if (message == WM_MOUSEMOVE || message == WM_NCMOUSEMOVE) {
        const auto parent = reinterpret_cast<HWND>(GetPropW(hwnd, kChildFrameHitTestParentProp));
        auto* self = parent ? reinterpret_cast<NebulaWindow*>(GetWindowLongPtrW(parent, GWLP_USERDATA)) : nullptr;
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

BOOL CALLBACK NebulaWindow::EnableFrameHitTestForDescendant(HWND hwnd, LPARAM lparam) {
    const auto* self = reinterpret_cast<const NebulaWindow*>(lparam);
    if (self) {
        self->EnableFrameHitTestForWindow(hwnd);
    }
    return TRUE;
}

LRESULT NebulaWindow::WndProc(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            UpdateDpi();
            if (delegate_) {
                delegate_->OnWindowCreated();
            }
            return 0;

        case WM_NCCALCSIZE:
            if (wparam == TRUE) {
                return 0;
            }
            break;

        case WM_NCACTIVATE:
            ApplyWindowFrameStyle(hwnd_);
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
            dpi_ = HIWORD(wparam);
            const auto* suggested_rect = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(
                hwnd_,
                nullptr,
                suggested_rect->left,
                suggested_rect->top,
                suggested_rect->right - suggested_rect->left,
                suggested_rect->bottom - suggested_rect->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            NotifyResize();
            return 0;
        }

        case WM_CLOSE:
            if (delegate_) {
                delegate_->OnWindowCloseRequested();
                return 0;
            }
            break;

        case WM_DESTROY:
            hwnd_ = nullptr;
            return 0;
    }

    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

void NebulaWindow::RegisterClass(HINSTANCE instance) {
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = StaticWndProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClassName;

    RegisterClassExW(&window_class);
}

void NebulaWindow::NotifyResize() {
    if (delegate_) {
        delegate_->OnWindowResized(CurrentLayout());
    }
}

void NebulaWindow::EnableFrameHitTestForWindow(HWND child) const {
    if (!child || GetPropW(child, kChildFrameHitTestOldProcProp)) {
        return;
    }

    SetPropW(child, kChildFrameHitTestParentProp, hwnd_);
    const auto old_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(child, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&NebulaWindow::ChildFrameWndProc)));
    if (old_proc) {
        SetPropW(child, kChildFrameHitTestOldProcProp, reinterpret_cast<HANDLE>(old_proc));
    } else {
        RemovePropW(child, kChildFrameHitTestParentProp);
    }
}

int NebulaWindow::ScaleForDpi(int value) const {
    return MulDiv(value, static_cast<int>(dpi_), 96);
}

void NebulaWindow::UpdateDpi() {
    if (hwnd_) {
        dpi_ = GetDpiForWindow(hwnd_);
    }
}

LRESULT NebulaWindow::HitTest(LPARAM lparam) const {
    if (!hwnd_) {
        return HTNOWHERE;
    }

    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    return HitTestPoint(point);
}

LRESULT NebulaWindow::HitTestPoint(POINT point) const {
    if (!hwnd_) {
        return HTNOWHERE;
    }

    RECT window = {};
    GetWindowRect(hwnd_, &window);

    const int resize_border = ScaleForDpi(resize_border_dip_);
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

}  // namespace nebula::window
