#pragma once

#include <string>
#include <vector>

#include "include/cef_browser.h"

namespace nebula::browser {

struct NebulaTab {
    int id = 1;
    std::string url;
    std::string title = "New Tab";
    bool is_loading = false;
    double load_progress = 0.0;
    std::string favicon_url;
    CefRefPtr<CefBrowser> browser;

    bool CanGoBack() const;
    bool CanGoForward() const;
};

}  // namespace nebula::browser
