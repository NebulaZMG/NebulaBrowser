#include "app/nebula_controller.h"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <system_error>

#include "browser/url_utils.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/wrapper/cef_helpers.h"
#include "ui/paths.h"

namespace nebula::app {
namespace {

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

CefWindowInfo ChildWindowInfo(HWND parent, const RECT& rect) {
    CefWindowInfo info;
    info.SetAsChild(
        parent,
        CefRect(
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top));
    return info;
}

CefBrowserSettings BrowserSettings() {
    CefBrowserSettings settings;
    settings.webgl = STATE_ENABLED;
    return settings;
}

int ParseTabId(const std::string& value) {
    int tab_id = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), tab_id);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size() ? tab_id : 0;
}

int ScaleForWindow(HWND hwnd, int value) {
    return MulDiv(value, static_cast<int>(GetDpiForWindow(hwnd)), 96);
}

RECT MenuPopupRect(HWND hwnd, const nebula::window::BrowserLayout& layout) {
    RECT client = {};
    GetClientRect(hwnd, &client);

    const int width = ScaleForWindow(hwnd, 260);
    const int height = ScaleForWindow(hwnd, 258);
    const int margin = ScaleForWindow(hwnd, 12);
    const int overlap = ScaleForWindow(hwnd, 2);

    const LONG x = std::max<LONG>(margin, client.right - width - margin);
    const LONG y = std::max<LONG>(0, layout.chrome.bottom - overlap);
    return {
        x,
        y,
        std::min<LONG>(client.right, x + width),
        std::min<LONG>(client.bottom, y + height),
    };
}

void ApplyRoundedWindowRegion(HWND hwnd, int corner_radius) {
    RECT rect = {};
    if (!hwnd || !GetClientRect(hwnd, &rect)) {
        return;
    }

    HRGN region = CreateRoundRectRgn(
        0,
        0,
        std::max<LONG>(1, rect.right - rect.left) + 1,
        std::max<LONG>(1, rect.bottom - rect.top) + 1,
        corner_radius,
        corner_radius);
    if (region && !SetWindowRgn(hwnd, region, TRUE)) {
        DeleteObject(region);
    }
}

std::string WithCacheBuster(std::string url) {
    if (url.empty()) {
        return url;
    }

    const size_t hash = url.find('#');
    std::string fragment;
    if (hash != std::string::npos) {
        fragment = url.substr(hash);
        url.resize(hash);
    }

    const char separator = url.find('?') == std::string::npos ? '?' : '&';
    return url + separator + "nebula_cache_bust=" + std::to_string(GetTickCount64()) + fragment;
}

std::string GetChromeDisplayUrl(const std::string& url) {
    return nebula::ui::IsInternalHomeUrl(url) ? std::string{} : url;
}

void SetBrowserVisible(CefRefPtr<CefBrowser> browser, bool visible) {
    if (!browser) {
        return;
    }

    const HWND hwnd = browser->GetHost()->GetWindowHandle();
    if (!hwnd) {
        return;
    }

    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
    if (visible) {
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

}  // namespace

NebulaController::NebulaController(HINSTANCE instance, std::string initial_url, int show_command)
    : instance_(instance),
      initial_url_(std::move(initial_url)),
      show_command_(show_command),
      tabs_(this) {}

NebulaController::~NebulaController() = default;

bool NebulaController::Create() {
    window_ = std::make_unique<nebula::window::NebulaWindow>(this);
    return window_->Create(instance_, show_command_);
}

void NebulaController::OnWindowCreated() {
    tabs_.CreateInitialTab(initial_url_.empty() ? nebula::ui::GetHomeUrl() : initial_url_);
    CreateChromeBrowser();
    CreateContentBrowser();
}

void NebulaController::OnWindowResized(const nebula::window::BrowserLayout& layout) {
    UNREFERENCED_PARAMETER(layout);
    ResizeBrowsers();
}

void NebulaController::OnWindowCloseRequested() {
    if (closing_) {
        MaybeFinishShutdown();
        return;
    }

    closing_ = true;
    if (chrome_browser_) {
        chrome_browser_->GetHost()->CloseBrowser(false);
    }
    if (menu_popup_browser_) {
        menu_popup_browser_->GetHost()->CloseBrowser(false);
    }
    for (const auto& tab : tabs_.Tabs()) {
        if (tab.browser) {
            tab.browser->GetHost()->CloseBrowser(false);
        }
    }
    MaybeFinishShutdown();
}

void NebulaController::OnActiveTabChanged(const nebula::browser::NebulaTab& tab) {
    if (chrome_ready_) {
        SendChromeState(tab);
    }
}

void NebulaController::OnBrowserCreated(nebula::cef::BrowserRole role, CefRefPtr<CefBrowser> browser) {
    if (window_ && browser) {
        window_->EnableFrameHitTest(browser->GetHost()->GetWindowHandle());
    }

    if (role == nebula::cef::BrowserRole::Chrome) {
        chrome_browser_ = browser;
        chrome_ready_ = true;
        if (const auto* tab = tabs_.ActiveTab()) {
            SendChromeState(*tab);
        }
    } else if (role == nebula::cef::BrowserRole::MenuPopup) {
        menu_popup_browser_ = browser;
        PositionMenuPopup();
    } else {
        tabs_.SetActiveBrowser(browser);
    }

    ResizeBrowsers();
}

void NebulaController::OnBrowserClosing(nebula::cef::BrowserRole role, CefRefPtr<CefBrowser> browser) {
    if (role == nebula::cef::BrowserRole::Chrome) {
        chrome_browser_ = nullptr;
        chrome_ready_ = false;
    } else if (role == nebula::cef::BrowserRole::MenuPopup) {
        menu_popup_browser_ = nullptr;
        menu_popup_client_ = nullptr;
    } else {
        tabs_.ClearBrowser(browser);
    }
    MaybeFinishShutdown();
}

void NebulaController::OnChromeCommand(const std::string& command, const std::string& payload) {
    if (command == "navigate") {
        tabs_.LoadURL(payload);
    } else if (command == "navigate-insecure") {
        const std::string target = nebula::browser::NormalizeNavigationInput(payload);
        if (nebula::ui::IsHttpUrl(target)) {
            insecure_warning_bypasses_.insert(target);
            tabs_.LoadURL(target);
        }
    } else if (command == "new-tab") {
        CreateNewTab();
    } else if (command == "activate-tab") {
        ActivateTab(ParseTabId(payload));
    } else if (command == "close-tab") {
        CloseTab(ParseTabId(payload));
    } else if (command == "back") {
        tabs_.GoBack();
    } else if (command == "forward") {
        tabs_.GoForward();
    } else if (command == "reload") {
        tabs_.Reload();
    } else if (command == "stop") {
        tabs_.StopLoad();
    } else if (command == "settings") {
        tabs_.LoadURL(nebula::ui::GetSettingsUrl());
    } else if (command == "menu-popup") {
        ToggleMenuPopup();
    } else if (command == "open-settings") {
        CloseMenuPopup();
        tabs_.LoadURL(nebula::ui::GetSettingsUrl());
    } else if (command == "big-picture") {
        CloseMenuPopup();
        tabs_.LoadURL(nebula::ui::GetBigPictureUrl());
    } else if (command == "gpu-diagnostics") {
        CloseMenuPopup();
        tabs_.LoadURL(nebula::ui::GetGpuDiagnosticsUrl());
    } else if (command == "toggle-devtools") {
        ToggleDevTools();
    } else if (command == "zoom-out") {
        AdjustZoom(-0.5);
    } else if (command == "zoom-in") {
        AdjustZoom(0.5);
    } else if (command == "hard-reload") {
        CloseMenuPopup();
        if (auto* tab = tabs_.ActiveTab(); tab && tab->browser) {
            tab->browser->ReloadIgnoreCache();
        }
    } else if (command == "fresh-reload") {
        CloseMenuPopup();
        FreshReload();
    } else if (command == "close-menu-popup") {
        CloseMenuPopup();
    } else if (command == "home") {
        tabs_.LoadURL(nebula::ui::GetHomeUrl());
    } else if (command == "minimize" && window_) {
        window_->Minimize();
    } else if (command == "maximize" && window_) {
        window_->ToggleMaximize();
    } else if (command == "close" && window_) {
        window_->Close();
    } else if (command == "drag" && window_) {
        window_->BeginDrag();
    }
}

void NebulaController::OnContentAddressChanged(CefRefPtr<CefBrowser> browser, const std::string& url) {
    tabs_.UpdateURL(browser,
                    nebula::ui::IsChromiumNewTabUrl(url)
                        ? nebula::ui::GetHomeUrl()
                        : nebula::ui::ToInternalUrl(url));
}

void NebulaController::OnContentTitleChanged(CefRefPtr<CefBrowser> browser, const std::string& title) {
    tabs_.UpdateTitle(browser, title);
    const auto* active_tab = tabs_.ActiveTab();
    if (window_ && active_tab && active_tab->browser && active_tab->browser->IsSame(browser)) {
        window_->SetTitle(Utf8ToWide(title.empty() ? "Nebula Browser" : title + " - Nebula"));
    }
}

void NebulaController::OnContentLoadingStateChanged(CefRefPtr<CefBrowser> browser, bool is_loading) {
    tabs_.UpdateLoadingState(browser, is_loading);
}

void NebulaController::OnContentLoadProgressChanged(CefRefPtr<CefBrowser> browser, double progress) {
    tabs_.UpdateLoadProgress(browser, progress);
}

void NebulaController::OnContentFaviconChanged(CefRefPtr<CefBrowser> browser, const std::vector<std::string>& urls) {
    tabs_.UpdateFavicon(browser, urls);
}

void NebulaController::OnPopupRequested(CefRefPtr<CefBrowser> browser, const std::string& target_url) {
    if (!tabs_.OwnsBrowser(browser)) {
        return;
    }

    tabs_.LoadURL(nebula::ui::IsEmptyOrChromiumNewTabUrl(target_url)
                      ? nebula::ui::GetHomeUrl()
                      : target_url);
}

bool NebulaController::ShouldBypassInsecureWarning(CefRefPtr<CefBrowser> browser, const std::string& target_url) {
    if (!tabs_.OwnsBrowser(browser)) {
        return false;
    }

    const auto bypass = insecure_warning_bypasses_.find(target_url);
    if (bypass == insecure_warning_bypasses_.end()) {
        return false;
    }

    insecure_warning_bypasses_.erase(bypass);
    return true;
}

void NebulaController::CreateNewTab() {
    if (auto* tab = tabs_.ActiveTab()) {
        SetBrowserVisible(tab->browser, false);
    }

    tabs_.CreateTab(nebula::ui::GetHomeUrl());
    CreateContentBrowser();
}

void NebulaController::ActivateTab(int tab_id) {
    auto* current_tab = tabs_.ActiveTab();
    if (current_tab && current_tab->id == tab_id) {
        return;
    }

    CefRefPtr<CefBrowser> previous_browser = current_tab ? current_tab->browser : nullptr;
    if (!tabs_.ActivateTab(tab_id)) {
        return;
    }

    SetBrowserVisible(previous_browser, false);
    if (auto* active_tab = tabs_.ActiveTab()) {
        if (active_tab->browser) {
            SetBrowserVisible(active_tab->browser, true);
        } else {
            CreateContentBrowser();
        }
    }
    ResizeBrowsers();
}

void NebulaController::CloseTab(int tab_id) {
    const bool was_active = [this, tab_id] {
        const auto* tab = tabs_.ActiveTab();
        return tab && tab->id == tab_id;
    }();

    CefRefPtr<CefBrowser> closing_browser = tabs_.CloseTab(tab_id);
    if (closing_browser) {
        closing_browser->GetHost()->CloseBrowser(false);
    }

    if (!tabs_.ActiveTab()) {
        tabs_.CreateTab(nebula::ui::GetHomeUrl());
        CreateContentBrowser();
        return;
    }

    if (was_active) {
        if (auto* active_tab = tabs_.ActiveTab()) {
            SetBrowserVisible(active_tab->browser, true);
        }
        ResizeBrowsers();
    }
}

void NebulaController::CreateChromeBrowser() {
    if (!window_ || !window_->hwnd()) {
        return;
    }

    const auto layout = window_->CurrentLayout();
    CefBrowserSettings browser_settings = BrowserSettings();
    chrome_client_ = new nebula::cef::NebulaBrowserClient(nebula::cef::BrowserRole::Chrome, this);
    CefWindowInfo window_info = ChildWindowInfo(window_->hwnd(), layout.chrome);
    CefBrowserHost::CreateBrowser(
        window_info, chrome_client_, nebula::ui::GetChromeUrl(), browser_settings, nullptr, nullptr);
}

void NebulaController::CreateContentBrowser() {
    if (!window_ || !window_->hwnd()) {
        return;
    }

    const auto* tab = tabs_.ActiveTab();
    const std::string url = tab && !tab->url.empty() ? tab->url : nebula::ui::GetHomeUrl();
    const auto layout = window_->CurrentLayout();
    CefBrowserSettings browser_settings = BrowserSettings();
    content_client_ = new nebula::cef::NebulaBrowserClient(nebula::cef::BrowserRole::Content, this);
    CefWindowInfo window_info = ChildWindowInfo(window_->hwnd(), layout.content);
    CefBrowserHost::CreateBrowser(
        window_info, content_client_, nebula::ui::ResolveInternalUrl(url), browser_settings, nullptr, nullptr);
}

void NebulaController::ToggleMenuPopup() {
    if (menu_popup_browser_) {
        CloseMenuPopup();
        return;
    }

    CreateMenuPopupBrowser();
}

void NebulaController::CloseMenuPopup() {
    if (menu_popup_browser_) {
        menu_popup_browser_->GetHost()->CloseBrowser(false);
    }
}

void NebulaController::CreateMenuPopupBrowser() {
    if (!window_ || !window_->hwnd()) {
        return;
    }

    const auto layout = window_->CurrentLayout();
    CefBrowserSettings browser_settings = BrowserSettings();
    menu_popup_client_ = new nebula::cef::NebulaBrowserClient(nebula::cef::BrowserRole::MenuPopup, this);
    CefWindowInfo window_info = ChildWindowInfo(window_->hwnd(), MenuPopupRect(window_->hwnd(), layout));
    CefBrowserHost::CreateBrowser(
        window_info, menu_popup_client_, nebula::ui::GetMenuPopupUrl(), browser_settings, nullptr, nullptr);
}

void NebulaController::PositionMenuPopup() {
    if (!window_ || !window_->hwnd() || !menu_popup_browser_) {
        return;
    }

    const auto rect = MenuPopupRect(window_->hwnd(), window_->CurrentLayout());
    const HWND hwnd = menu_popup_browser_->GetHost()->GetWindowHandle();
    window_->ResizeChild(hwnd, rect);
    ApplyRoundedWindowRegion(hwnd, ScaleForWindow(window_->hwnd(), 28));
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void NebulaController::ToggleDevTools() {
    auto* tab = tabs_.ActiveTab();
    if (!tab || !tab->browser || !window_ || !window_->hwnd()) {
        return;
    }

    CefRefPtr<CefBrowserHost> host = tab->browser->GetHost();
    if (host->HasDevTools()) {
        host->CloseDevTools();
        return;
    }

    CefWindowInfo window_info;
    window_info.SetAsPopup(window_->hwnd(), "Nebula Developer Tools");
    CefBrowserSettings browser_settings;
    host->ShowDevTools(window_info, content_client_, browser_settings, CefPoint());
}

void NebulaController::AdjustZoom(double delta) {
    auto* tab = tabs_.ActiveTab();
    if (!tab || !tab->browser) {
        return;
    }

    CefRefPtr<CefBrowserHost> host = tab->browser->GetHost();
    host->SetZoomLevel(host->GetZoomLevel() + delta);
}

void NebulaController::FreshReload() {
    auto* tab = tabs_.ActiveTab();
    if (!tab || tab->url.empty()) {
        return;
    }

    tabs_.LoadURL(WithCacheBuster(tab->url));
}

void NebulaController::ResizeBrowsers() {
    if (!window_) {
        return;
    }

    const auto layout = window_->CurrentLayout();
    if (chrome_browser_) {
        window_->ResizeChild(chrome_browser_->GetHost()->GetWindowHandle(), layout.chrome);
    }
    if (const auto* tab = tabs_.ActiveTab(); tab && tab->browser) {
        window_->ResizeChild(tab->browser->GetHost()->GetWindowHandle(), layout.content);
    }
    PositionMenuPopup();
}

void NebulaController::SendChromeState(const nebula::browser::NebulaTab& tab) {
    if (!chrome_browser_) {
        return;
    }

    const std::string display_url = GetChromeDisplayUrl(tab.url);
    std::string tabs_json = "[";
    const auto& tabs = tabs_.Tabs();
    for (size_t i = 0; i < tabs.size(); ++i) {
        const auto& item = tabs[i];
        if (i > 0) {
            tabs_json += ",";
        }
        tabs_json +=
            "{\"id\":" + std::to_string(item.id) +
            ",\"title\":\"" + nebula::browser::JsonEscape(item.title) + "\"" +
            ",\"isLoading\":" + std::string(item.is_loading ? "true" : "false") +
            ",\"favicon\":\"" + nebula::browser::JsonEscape(item.favicon_url) + "\"" +
            "}";
    }
    tabs_json += "]";

    const std::string script =
        "window.NebulaChrome && window.NebulaChrome.applyState({"
        "\"id\":" + std::to_string(tab.id) +
        ",\"url\":\"" + nebula::browser::JsonEscape(display_url) + "\""
        ",\"title\":\"" + nebula::browser::JsonEscape(tab.title) + "\""
        ",\"isLoading\":" + std::string(tab.is_loading ? "true" : "false") +
        ",\"progress\":" + std::to_string(tab.load_progress) +
        ",\"canGoBack\":" + std::string(tab.CanGoBack() ? "true" : "false") +
        ",\"canGoForward\":" + std::string(tab.CanGoForward() ? "true" : "false") +
        ",\"favicon\":\"" + nebula::browser::JsonEscape(tab.favicon_url) + "\"" +
        ",\"tabs\":" + tabs_json +
        "});";

    chrome_browser_->GetMainFrame()->ExecuteJavaScript(script, nebula::ui::GetChromeUrl(), 0);
}

void NebulaController::MaybeFinishShutdown() {
    if (!closing_) {
        return;
    }

    if (chrome_browser_ || menu_popup_browser_ || tabs_.HasOpenBrowsers()) {
        return;
    }

    if (window_ && window_->hwnd()) {
        DestroyWindow(window_->hwnd());
    }
    CefQuitMessageLoop();
}

}  // namespace nebula::app
