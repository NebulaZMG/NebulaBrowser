#include "platform/browser_host.h"

#include <algorithm>

namespace nebula::platform {

CefWindowInfo MakeChildWindowInfo(NativeWindow parent, const Rect& rect) {
    CefWindowInfo info;
    info.SetAsChild(
        reinterpret_cast<CefWindowHandle>(parent),
        CefRect(rect.x, rect.y, rect.width, rect.height));
    info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    return info;
}

CefWindowInfo MakeDevToolsPopup(NativeWindow parent, const char* title) {
    CefWindowInfo info;
    info.SetAsPopup(reinterpret_cast<CefWindowHandle>(parent), title);
    info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    return info;
}

void ResizeBrowserWindow(NativeWindow browser_window, const Rect& rect) {
    UNREFERENCED_PARAMETER(browser_window);
    UNREFERENCED_PARAMETER(rect);
}

void SetBrowserVisible(NativeWindow browser_window, bool visible) {
    UNREFERENCED_PARAMETER(browser_window);
    UNREFERENCED_PARAMETER(visible);
}

void RaiseBrowserWindow(NativeWindow browser_window) {
    UNREFERENCED_PARAMETER(browser_window);
}

int ScaleForParentWindow(NativeWindow parent, int value) {
    UNREFERENCED_PARAMETER(parent);
    return value;
}

std::pair<int, int> ParentClientSize(NativeWindow parent) {
    UNREFERENCED_PARAMETER(parent);
    return {1280, 720};
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
    return "0";
}

void DestroyTopLevelWindow(NativeWindow window) {
    UNREFERENCED_PARAMETER(window);
}

}  // namespace nebula::platform
