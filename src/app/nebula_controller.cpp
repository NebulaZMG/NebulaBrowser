#include "app/nebula_controller.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "browser/session_state.h"
#include "browser/url_utils.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_cookie.h"
#include "include/wrapper/cef_helpers.h"
#include "platform/browser_host.h"
#include "ui/paths.h"

namespace nebula::app {
namespace {

constexpr size_t kMaxSiteHistoryEntries = 200;

std::filesystem::path GetSiteHistoryPath() {
    const auto user_data = nebula::ui::GetUserDataDirectory();
    return user_data.empty() ? std::filesystem::path{} : user_data / L"site_history.txt";
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool IsSiteHistoryUrl(const std::string& url) {
    const std::string lower = ToLowerAscii(url);
    return lower.starts_with("http://") || lower.starts_with("https://");
}

std::vector<std::string> LoadSiteHistory() {
    std::vector<std::string> history;
    std::ifstream input(GetSiteHistoryPath(), std::ios::binary);
    if (!input) {
        return history;
    }

    std::string url;
    while (std::getline(input, url) && history.size() < kMaxSiteHistoryEntries) {
        if (IsSiteHistoryUrl(url)) {
            history.push_back(url);
        }
    }
    return history;
}

void SaveSiteHistory(const std::vector<std::string>& history) {
    const auto path = GetSiteHistoryPath();
    if (path.empty()) {
        return;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return;
    }

    for (const auto& url : history) {
        output << url << '\n';
    }
}

std::string SiteHistoryJson(const std::vector<std::string>& history) {
    std::string json = "[";
    for (size_t i = 0; i < history.size(); ++i) {
        if (i > 0) {
            json += ",";
        }
        json += "\"" + nebula::browser::JsonEscape(history[i]) + "\"";
    }
    json += "]";
    return json;
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
    return url + separator + "nebula_cache_bust=" + nebula::platform::CacheBusterToken() + fragment;
}

std::string GetChromeDisplayUrl(const std::string& url) {
    return nebula::ui::IsInternalHomeUrl(url) ? std::string{} : url;
}

void SetBrowserVisible(CefRefPtr<CefBrowser> browser, bool visible) {
    if (!browser) {
        return;
    }

    nebula::platform::SetBrowserVisible(browser->GetHost()->GetWindowHandle(), visible);
}

}  // namespace

NebulaController::NebulaController(nebula::platform::AppStartup startup, std::string initial_url)
    : startup_(startup),
      initial_url_(std::move(initial_url)),
      tabs_(this),
      site_history_(LoadSiteHistory()) {}

NebulaController::~NebulaController() = default;

bool NebulaController::Create() {
    window_ = std::make_unique<nebula::window::NebulaWindow>(this);
    return window_->Create(startup_);
}

void NebulaController::OnWindowCreated() {
    if (initial_url_.empty()) {
        tabs_.CreateInitialTab(nebula::ui::GetHomeUrl());
    } else {
        tabs_.CreateInitialTab(initial_url_);
    }
    PersistSession();

    CreateChromeBrowser();
    CreateContentBrowser();
}

void NebulaController::OnWindowResized(const nebula::window::BrowserLayout& layout) {
    UNREFERENCED_PARAMETER(layout);
    ResizeBrowsers();
}

void NebulaController::OnWindowCloseRequested() {
    if (!closing_ && !closing_tab_browsers_.empty()) {
        // CEF Alloy can bubble a child browser close as WM_CLOSE on the host
        // window. Per-tab closes should not turn into full app shutdown.
        return;
    }

    if (closing_) {
        if (window_ && window_->native_handle()) {
            nebula::platform::DestroyTopLevelWindow(window_->native_handle());
        }
        MaybeFinishShutdown();
        return;
    }

    closing_ = true;
    PersistSession();
    if (auto cookie_manager = CefCookieManager::GetGlobalManager(nullptr)) {
        cookie_manager->FlushStore(nullptr);
    }

    if (chrome_browser_) {
        chrome_browser_->GetHost()->CloseBrowser(true);
    }
    if (menu_popup_browser_) {
        menu_popup_browser_->GetHost()->CloseBrowser(true);
    }
    for (const auto& tab : tabs_.Tabs()) {
        if (tab.browser) {
            tab.browser->GetHost()->CloseBrowser(true);
        }
    }

    // Do not wait for CEF to re-send WM_CLOSE to the host window. On some
    // Alloy child-window paths that message never arrives, leaving the app
    // alive with all close affordances disabled until the process is killed.
    if (window_ && window_->native_handle()) {
        nebula::platform::DestroyTopLevelWindow(window_->native_handle());
    }
    MaybeFinishShutdown();
}

void NebulaController::OnActiveTabChanged(const nebula::browser::NebulaTab& tab) {
    if (chrome_ready_) {
        SendChromeState(tab);
    }
}

void NebulaController::OnBrowserCreated(nebula::cef::BrowserRole role, CefRefPtr<CefBrowser> browser) {
    if (window_ && browser && role != nebula::cef::BrowserRole::MenuPopup) {
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
        menu_popup_visible_ = true;
        PositionMenuPopup();
        SendMenuPopupZoom();
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
        menu_popup_visible_ = false;
    } else {
        ForgetClosingTabBrowser(browser);
        if (content_fullscreen_) {
            const auto* active_tab = tabs_.ActiveTab();
            if (active_tab && active_tab->browser && active_tab->browser->IsSame(browser)) {
                SetContentFullscreen(false);
            }
        }
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
        CreateNewTab(payload);
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
    } else if (command == "clear-site-history") {
        site_history_.clear();
        SaveSiteHistory(site_history_);
        if (auto* tab = tabs_.ActiveTab(); tab && tab->browser) {
            InjectSettingsHistory(tab->browser);
        }
    } else if (command == "minimize" && window_) {
        window_->Minimize();
    } else if (command == "maximize" && window_) {
        window_->ToggleMaximize();
    } else if (command == "close" && window_) {
        OnWindowCloseRequested();
    } else if (command == "exit-bigpicture" && window_) {
        OnWindowCloseRequested();
    } else if (command == "drag" && window_) {
        window_->BeginDrag();
    }
}

void NebulaController::OnContentAddressChanged(CefRefPtr<CefBrowser> browser, const std::string& url) {
    const std::string internal_url = nebula::ui::ToInternalUrl(url);
    tabs_.UpdateURL(browser,
                    nebula::ui::IsChromiumNewTabUrl(url)
                        ? nebula::ui::GetHomeUrl()
                        : internal_url);
    RecordSiteHistory(internal_url);
    PersistSession();
}

void NebulaController::OnContentTitleChanged(CefRefPtr<CefBrowser> browser, const std::string& title) {
    tabs_.UpdateTitle(browser, title);
    PersistSession();
    const auto* active_tab = tabs_.ActiveTab();
    if (window_ && active_tab && active_tab->browser && active_tab->browser->IsSame(browser)) {
        window_->SetTitle(title.empty() ? "Nebula Browser" : title + " - Nebula");
    }
}

void NebulaController::OnContentLoadingStateChanged(CefRefPtr<CefBrowser> browser, bool is_loading) {
    tabs_.UpdateLoadingState(browser, is_loading);
}

void NebulaController::OnContentLoadProgressChanged(CefRefPtr<CefBrowser> browser, double progress) {
    tabs_.UpdateLoadProgress(browser, progress);
}

void NebulaController::OnContentLoadFinished(CefRefPtr<CefBrowser> browser, const std::string& url) {
    if (nebula::ui::ToInternalUrl(url).starts_with(nebula::ui::GetSettingsUrl())) {
        InjectSettingsHistory(browser);
    }
}

void NebulaController::OnContentFaviconChanged(CefRefPtr<CefBrowser> browser, const std::vector<std::string>& urls) {
    tabs_.UpdateFavicon(browser, urls);
}

void NebulaController::OnContentFullscreenChanged(CefRefPtr<CefBrowser> browser, bool fullscreen) {
    const auto* active_tab = tabs_.ActiveTab();
    if (!active_tab || !active_tab->browser || !active_tab->browser->IsSame(browser)) {
        return;
    }

    SetContentFullscreen(fullscreen);
}

void NebulaController::OnPopupRequested(CefRefPtr<CefBrowser> browser, const std::string& target_url) {
    if (!tabs_.OwnsBrowser(browser)) {
        return;
    }

    CreateNewTab(nebula::ui::IsEmptyOrChromiumNewTabUrl(target_url)
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

void NebulaController::CreateNewTab(std::string url) {
    if (auto* tab = tabs_.ActiveTab()) {
        SetBrowserVisible(tab->browser, false);
    }

    const std::string target =
        url.empty() ? nebula::ui::GetHomeUrl() : nebula::browser::NormalizeNavigationInput(url);
    tabs_.CreateTab(target.empty() ? nebula::ui::GetHomeUrl() : target);
    PersistSession();
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
    PersistSession();

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
    PersistSession();
    if (closing_browser) {
        closing_tab_browsers_.push_back(closing_browser);
        closing_browser->GetHost()->CloseBrowser(false);
    }

    if (!tabs_.ActiveTab()) {
        tabs_.CreateTab(nebula::ui::GetHomeUrl());
        PersistSession();
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
    if (!window_ || !window_->native_handle()) {
        return;
    }

    const auto layout = window_->CurrentLayout();
    CefBrowserSettings browser_settings = BrowserSettings();
    chrome_client_ = new nebula::cef::NebulaBrowserClient(nebula::cef::BrowserRole::Chrome, this);
    CefWindowInfo window_info =
        nebula::platform::MakeChildWindowInfo(window_->native_handle(), layout.chrome);
    CefBrowserHost::CreateBrowser(
        window_info, chrome_client_, nebula::ui::GetChromeUrl(), browser_settings, nullptr, nullptr);
}

void NebulaController::CreateContentBrowser() {
    if (!window_ || !window_->native_handle()) {
        return;
    }

    const auto* tab = tabs_.ActiveTab();
    const std::string url = tab && !tab->url.empty() ? tab->url : nebula::ui::GetHomeUrl();
    const auto layout = window_->CurrentLayout();
    CefBrowserSettings browser_settings = BrowserSettings();
    content_client_ = new nebula::cef::NebulaBrowserClient(nebula::cef::BrowserRole::Content, this);
    CefWindowInfo window_info =
        nebula::platform::MakeChildWindowInfo(window_->native_handle(), layout.content);
    CefBrowserHost::CreateBrowser(
        window_info, content_client_, nebula::ui::ResolveInternalUrl(url), browser_settings, nullptr, nullptr);
}

void NebulaController::ToggleMenuPopup() {
    if (menu_popup_browser_ && menu_popup_visible_) {
        CloseMenuPopup();
        return;
    }

    if (menu_popup_browser_) {
        menu_popup_visible_ = true;
        PositionMenuPopup();
        SetBrowserVisible(menu_popup_browser_, true);
        SendMenuPopupZoom();
        return;
    }

    CreateMenuPopupBrowser();
}

void NebulaController::CloseMenuPopup() {
    if (menu_popup_browser_) {
        menu_popup_visible_ = false;
        SetBrowserVisible(menu_popup_browser_, false);
    }
}

void NebulaController::CreateMenuPopupBrowser() {
    if (!window_ || !window_->native_handle() || content_fullscreen_) {
        return;
    }

    const auto layout = window_->CurrentLayout();
    CefBrowserSettings browser_settings = BrowserSettings();
    menu_popup_client_ = new nebula::cef::NebulaBrowserClient(nebula::cef::BrowserRole::MenuPopup, this);
    CefWindowInfo window_info = nebula::platform::MakeChildWindowInfo(
        window_->native_handle(),
        nebula::platform::MenuPopupRect(window_->native_handle(), layout));
    CefBrowserHost::CreateBrowser(
        window_info, menu_popup_client_, nebula::ui::GetMenuPopupUrl(), browser_settings, nullptr, nullptr);
}

void NebulaController::PositionMenuPopup() {
    if (content_fullscreen_ || !window_ || !window_->native_handle() || !menu_popup_browser_ ||
        !menu_popup_visible_) {
        return;
    }

    const auto rect =
        nebula::platform::MenuPopupRect(window_->native_handle(), window_->CurrentLayout());
    const auto browser_window = menu_popup_browser_->GetHost()->GetWindowHandle();
    nebula::platform::ResizeBrowserWindow(browser_window, rect);
    nebula::platform::RaiseBrowserWindow(browser_window);
}

void NebulaController::SendMenuPopupZoom() {
    if (!menu_popup_browser_) {
        return;
    }

    double zoom_level = 0.0;
    if (const auto* tab = tabs_.ActiveTab(); tab && tab->browser) {
        zoom_level = tab->browser->GetHost()->GetZoomLevel();
    }

    const std::string script =
        "window.NebulaMenuPopup && window.NebulaMenuPopup.setZoomLevel(" +
        std::to_string(zoom_level) + ");";
    menu_popup_browser_->GetMainFrame()->ExecuteJavaScript(script, nebula::ui::GetMenuPopupUrl(), 0);
}

void NebulaController::ToggleDevTools() {
    auto* tab = tabs_.ActiveTab();
    if (!tab || !tab->browser || !window_ || !window_->native_handle()) {
        return;
    }

    CefRefPtr<CefBrowserHost> host = tab->browser->GetHost();
    if (host->HasDevTools()) {
        host->CloseDevTools();
        return;
    }

    CefWindowInfo window_info =
        nebula::platform::MakeDevToolsPopup(window_->native_handle(), "Nebula Developer Tools");
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
    SendMenuPopupZoom();
    if (chrome_ready_) {
        SendChromeState(*tab);
    }
}

void NebulaController::FreshReload() {
    auto* tab = tabs_.ActiveTab();
    if (!tab || tab->url.empty()) {
        return;
    }

    tabs_.LoadURL(WithCacheBuster(tab->url));
}

void NebulaController::SetContentFullscreen(bool fullscreen) {
    if (content_fullscreen_ == fullscreen) {
        return;
    }

    content_fullscreen_ = fullscreen;
    if (fullscreen) {
        CloseMenuPopup();
    }

    SetBrowserVisible(chrome_browser_, !fullscreen);
    if (window_) {
        window_->SetFullscreen(fullscreen);
    }
    ResizeBrowsers();
}

void NebulaController::ResizeBrowsers() {
    if (!window_) {
        return;
    }

    const auto layout = window_->CurrentLayout(!content_fullscreen_);
    if (chrome_browser_) {
        window_->ResizeChild(
            chrome_browser_->GetHost()->GetWindowHandle(),
            layout.chrome);
    }
    if (const auto* tab = tabs_.ActiveTab(); tab && tab->browser) {
        window_->ResizeChild(
            tab->browser->GetHost()->GetWindowHandle(),
            layout.content);
    }
    if (!content_fullscreen_) {
        PositionMenuPopup();
    }
}

void NebulaController::SendChromeState(const nebula::browser::NebulaTab& tab) {
    if (!chrome_browser_) {
        return;
    }

    const std::string display_url = GetChromeDisplayUrl(tab.url);
    double zoom_level = 0.0;
    if (tab.browser) {
        zoom_level = tab.browser->GetHost()->GetZoomLevel();
    }
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
        ",\"zoomLevel\":" + std::to_string(zoom_level) +
        ",\"tabs\":" + tabs_json +
        "});";

    chrome_browser_->GetMainFrame()->ExecuteJavaScript(script, nebula::ui::GetChromeUrl(), 0);
}

void NebulaController::RecordSiteHistory(const std::string& url) {
    if (!IsSiteHistoryUrl(url)) {
        return;
    }

    site_history_.erase(
        std::remove(site_history_.begin(), site_history_.end(), url),
        site_history_.end());
    site_history_.insert(site_history_.begin(), url);
    if (site_history_.size() > kMaxSiteHistoryEntries) {
        site_history_.resize(kMaxSiteHistoryEntries);
    }
    SaveSiteHistory(site_history_);
}

void NebulaController::InjectSettingsHistory(CefRefPtr<CefBrowser> browser) {
    if (!browser) {
        return;
    }

    const std::string history_json = SiteHistoryJson(site_history_);
    const std::string script =
        "localStorage.setItem('siteHistory', \"" + nebula::browser::JsonEscape(history_json) + "\");"
        "if (typeof loadHistories === 'function') { loadHistories(); }";
    browser->GetMainFrame()->ExecuteJavaScript(script, nebula::ui::GetSettingsUrl(), 0);
}

void NebulaController::PersistSession() const {
    nebula::browser::SaveSessionState(tabs_.Tabs(), tabs_.ActiveTabIndex());
}

void NebulaController::MaybeFinishShutdown() {
    if (!closing_) {
        return;
    }

    if (chrome_browser_ || menu_popup_browser_ || tabs_.HasOpenBrowsers()) {
        return;
    }

    if (window_ && window_->native_handle()) {
        nebula::platform::DestroyTopLevelWindow(window_->native_handle());
    }
    CefQuitMessageLoop();
}

bool NebulaController::ForgetClosingTabBrowser(CefRefPtr<CefBrowser> browser) {
    if (!browser) {
        return false;
    }

    const auto it = std::find_if(
        closing_tab_browsers_.begin(),
        closing_tab_browsers_.end(),
        [browser](const CefRefPtr<CefBrowser>& closing_browser) {
            return closing_browser && closing_browser->IsSame(browser);
        });
    if (it == closing_tab_browsers_.end()) {
        return false;
    }

    closing_tab_browsers_.erase(it);
    return true;
}

}  // namespace nebula::app
