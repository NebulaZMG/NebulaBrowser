#include "platform/browser_host.h"

#include <windows.h>

#include <algorithm>

namespace nebula::platform {
namespace {

HWND AsHwnd(NativeWindow window) {
    return static_cast<HWND>(window);
}

RECT ToRect(const Rect& rect) {
    return {
        rect.x,
        rect.y,
        rect.x + rect.width,
        rect.y + rect.height,
    };
}

}  // namespace

CefWindowInfo MakeChildWindowInfo(NativeWindow parent, const Rect& rect) {
    CefWindowInfo info;
    info.SetAsChild(
        AsHwnd(parent),
        CefRect(rect.x, rect.y, rect.width, rect.height));
    info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    return info;
}

CefWindowInfo MakeDevToolsPopup(NativeWindow parent, const char* title) {
    CefWindowInfo info;
    info.SetAsPopup(AsHwnd(parent), title);
    info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    return info;
}

void ResizeBrowserWindow(NativeWindow browser_window, const Rect& rect) {
    const HWND hwnd = AsHwnd(browser_window);
    if (!hwnd) {
        return;
    }

    SetWindowPos(
        hwnd,
        nullptr,
        rect.x,
        rect.y,
        std::max(0, rect.width),
        std::max(0, rect.height),
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void SetBrowserVisible(NativeWindow browser_window, bool visible) {
    const HWND hwnd = AsHwnd(browser_window);
    if (!hwnd) {
        return;
    }

    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
    if (visible) {
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void RaiseBrowserWindow(NativeWindow browser_window) {
    const HWND hwnd = AsHwnd(browser_window);
    if (!hwnd) {
        return;
    }

    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

int ScaleForParentWindow(NativeWindow parent, int value) {
    const HWND hwnd = AsHwnd(parent);
    if (!hwnd) {
        return value;
    }

    return MulDiv(value, static_cast<int>(GetDpiForWindow(hwnd)), 96);
}

std::pair<int, int> ParentClientSize(NativeWindow parent) {
    RECT client = {};
    const HWND hwnd = AsHwnd(parent);
    if (hwnd) {
        GetClientRect(hwnd, &client);
    }

    return {static_cast<int>(client.right), static_cast<int>(client.bottom)};
}

Rect MenuPopupRect(NativeWindow parent, const BrowserLayout& layout) {
    const auto client_size = ParentClientSize(parent);
    const int width = ScaleForParentWindow(parent, 260);
    const int height = ScaleForParentWindow(parent, 258);
    const int margin = ScaleForParentWindow(parent, 12);
    const int overlap = ScaleForParentWindow(parent, 2);

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
    return std::to_string(GetTickCount64());
}

void DestroyTopLevelWindow(NativeWindow window) {
    const HWND hwnd = AsHwnd(window);
    if (hwnd) {
        DestroyWindow(hwnd);
    }
}

}  // namespace nebula::platform
