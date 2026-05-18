#pragma once

#include <string>
#include <utility>

#include "include/cef_browser.h"
#include "platform/types.h"

namespace nebula::platform {

CefWindowInfo MakeChildWindowInfo(NativeWindow parent, const Rect& rect);
CefWindowInfo MakeDevToolsPopup(NativeWindow parent, const char* title);
void ResizeBrowserWindow(NativeWindow browser_window, const Rect& rect);
void SetBrowserVisible(NativeWindow browser_window, bool visible);
void RaiseBrowserWindow(NativeWindow browser_window);
Rect MenuPopupRect(NativeWindow parent, const BrowserLayout& layout);
std::string CacheBusterToken();
void DestroyTopLevelWindow(NativeWindow window);
int ScaleForParentWindow(NativeWindow parent, int value);
std::pair<int, int> ParentClientSize(NativeWindow parent);

}  // namespace nebula::platform
