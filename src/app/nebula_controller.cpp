#include "app/nebula_controller.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "app/first_run_state.h"
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

bool ParseTwoInts(const std::string& value, int& first, int& second) {
    const size_t separator = value.find(',');
    if (separator == std::string::npos) {
        return false;
    }

    const std::string first_value = value.substr(0, separator);
    const std::string second_value = value.substr(separator + 1);
    const auto first_result =
        std::from_chars(first_value.data(), first_value.data() + first_value.size(), first);
    const auto second_result =
        std::from_chars(second_value.data(), second_value.data() + second_value.size(), second);
    return first_result.ec == std::errc{} && first_result.ptr == first_value.data() + first_value.size() &&
           second_result.ec == std::errc{} && second_result.ptr == second_value.data() + second_value.size();
}

bool ParseFourInts(const std::string& value, int& first, int& second, int& third, int& fourth) {
    const size_t first_separator = value.find(',');
    if (first_separator == std::string::npos) {
        return false;
    }
    const size_t second_separator = value.find(',', first_separator + 1);
    if (second_separator == std::string::npos) {
        return false;
    }
    const size_t third_separator = value.find(',', second_separator + 1);
    if (third_separator == std::string::npos) {
        return false;
    }

    const std::string first_value = value.substr(0, first_separator);
    const std::string second_value = value.substr(first_separator + 1, second_separator - first_separator - 1);
    const std::string third_value = value.substr(second_separator + 1, third_separator - second_separator - 1);
    const std::string fourth_value = value.substr(third_separator + 1);
    const auto first_result =
        std::from_chars(first_value.data(), first_value.data() + first_value.size(), first);
    const auto second_result =
        std::from_chars(second_value.data(), second_value.data() + second_value.size(), second);
    const auto third_result =
        std::from_chars(third_value.data(), third_value.data() + third_value.size(), third);
    const auto fourth_result =
        std::from_chars(fourth_value.data(), fourth_value.data() + fourth_value.size(), fourth);
    return first_result.ec == std::errc{} && first_result.ptr == first_value.data() + first_value.size() &&
           second_result.ec == std::errc{} && second_result.ptr == second_value.data() + second_value.size() &&
           third_result.ec == std::errc{} && third_result.ptr == third_value.data() + third_value.size() &&
           fourth_result.ec == std::errc{} && fourth_result.ptr == fourth_value.data() + fourth_value.size();
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

NebulaController::NebulaController(nebula::platform::AppStartup startup,
                                   std::string initial_url,
                                   LaunchOptions launch_options)
    : startup_(startup),
      initial_url_(std::move(initial_url)),
      launch_options_(launch_options),
      tabs_(this),
      site_history_(LoadSiteHistory()) {}

NebulaController::~NebulaController() = default;

bool NebulaController::Create() {
    window_ = std::make_unique<nebula::window::NebulaWindow>(this);
    return window_->Create(startup_);
}

void NebulaController::OnWindowCreated() {
    big_picture_mode_ = launch_options_.mode == AppMode::BigPicture;
    if (big_picture_mode_ && window_) {
        window_->SetFullscreen(true);
    }

    first_run_setup_active_ =
        !big_picture_mode_ && initial_url_.empty() && ShouldShowFirstRunSetup();

    if (first_run_setup_active_) {
        tabs_.CreateInitialTab(nebula::ui::GetSetupUrl());
    } else if (initial_url_.empty()) {
        tabs_.CreateInitialTab(nebula::ui::GetHomeUrl());
    } else {
        tabs_.CreateInitialTab(initial_url_);
    }
    PersistSession();

    if (!big_picture_mode_ && !first_run_setup_active_) {
        CreateChromeBrowser();
    }
    CreateContentBrowser();
    if (big_picture_mode_) {
        CreateBigPictureBrowser();
    }
}

void NebulaController::OnWindowResized(const nebula::window::BrowserLayout& layout) {
    (void)layout;
    ResizeBrowsers();
}

void NebulaController::OnWindowCloseRequested() {
    if (!closing_ && !closing_tab_browsers_.empty()) {
        // CEF Alloy can bubble a child browser close as WM_CLOSE on the host
        // window. Per-tab closes should not turn into full app shutdown.
        return;
    }

    BeginShutdown();
}

void NebulaController::BeginShutdown() {
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

    std::vector<CefRefPtr<CefBrowser>> content_browsers = closing_tab_browsers_;
    for (const auto& tab : tabs_.Tabs()) {
        if (tab.browser) {
            content_browsers.push_back(tab.browser);
        }
    }

    if (chrome_browser_) {
        chrome_browser_->GetHost()->CloseBrowser(true);
    }
    if (big_picture_browser_) {
        big_picture_browser_->GetHost()->CloseBrowser(true);
    }
    if (menu_popup_browser_) {
        menu_popup_browser_->GetHost()->CloseBrowser(true);
    }
    for (const auto& browser : content_browsers) {
        if (browser) {
            browser->GetHost()->CloseBrowser(true);
        }
    }

    // Do not wait for CEF to re-send WM_CLOSE to the host window. On some
    // Alloy child-window paths that message never arrives, so the controller
    // finishes shutdown once every CEF browser has reported OnBeforeClose.
    MaybeFinishShutdown();
}

void NebulaController::OnActiveTabChanged(const nebula::browser::NebulaTab& tab) {
    if (chrome_ready_) {
        SendChromeState(tab);
    }
    if (big_picture_ready_) {
        SendBigPictureState(tab);
    }
    if (big_picture_mode_) {
        InjectBigPictureCursor(tab.browser);
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
    } else if (role == nebula::cef::BrowserRole::BigPicture) {
        big_picture_browser_ = browser;
        big_picture_ready_ = true;
        SetBrowserVisible(big_picture_browser_, big_picture_mode_);
        if (const auto* tab = tabs_.ActiveTab()) {
            SendBigPictureState(*tab);
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
    } else if (role == nebula::cef::BrowserRole::BigPicture) {
        big_picture_browser_ = nullptr;
        big_picture_client_ = nullptr;
        big_picture_ready_ = false;
        big_picture_mode_ = false;
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
        EnterBigPictureMode();
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
    } else if (command == "theme-update") {
        SendThemeToChromeSurfaces(payload);
    } else if (command == "complete-first-run") {
        CompleteFirstRunSetup();
    } else if (command == "clear-site-history") {
        site_history_.clear();
        SaveSiteHistory(site_history_);
        if (auto* tab = tabs_.ActiveTab(); tab && tab->browser) {
            InjectSettingsHistory(tab->browser);
        }
        if (auto* tab = tabs_.ActiveTab()) {
            SendBigPictureState(*tab);
        }
    } else if (command == "clear-search-history") {
        if (auto* tab = tabs_.ActiveTab()) {
            SendBigPictureState(*tab);
        }
    } else if (command == "bigpicture-mouse-move") {
        SendBigPictureMouseMove(payload);
    } else if (command == "bigpicture-click") {
        SendBigPictureMouseClick(payload, false);
    } else if (command == "bigpicture-right-click") {
        SendBigPictureMouseClick(payload, true);
    } else if (command == "bigpicture-scroll") {
        SendBigPictureMouseWheel(payload);
    } else if (command == "bigpicture-text") {
        SendBigPictureText(payload);
    } else if (command == "bigpicture-browse-visible") {
        SetBigPictureBrowseVisible(payload == "1" || payload == "true");
    } else if (command == "minimize" && window_) {
        window_->Minimize();
    } else if (command == "maximize" && window_) {
        window_->ToggleMaximize();
    } else if (command == "close" && window_) {
        BeginShutdown();
    } else if (command == "exit-bigpicture" && window_) {
        if (launch_options_.mode == AppMode::BigPicture) {
            BeginShutdown();
            return;
        }
        ExitBigPictureMode();
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
    if (const auto* active_tab = tabs_.ActiveTab()) {
        SendBigPictureState(*active_tab);
    }
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
    InjectBigPictureCursor(browser);
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

    const auto layout = CurrentBrowserLayout();
    CefBrowserSettings browser_settings = BrowserSettings();
    chrome_client_ = new nebula::cef::NebulaBrowserClient(nebula::cef::BrowserRole::Chrome, this);
    CefWindowInfo window_info =
        nebula::platform::MakeChildWindowInfo(window_->native_handle(), layout.chrome);
    CefBrowserHost::CreateBrowser(
        window_info, chrome_client_, nebula::ui::GetChromeUrl(), browser_settings, nullptr, nullptr);
}

void NebulaController::CreateBigPictureBrowser() {
    if (!window_ || !window_->native_handle()) {
        return;
    }

    const auto layout = CurrentBrowserLayout();
    CefBrowserSettings browser_settings = BrowserSettings();
    big_picture_client_ = new nebula::cef::NebulaBrowserClient(nebula::cef::BrowserRole::BigPicture, this);
    CefWindowInfo window_info =
        nebula::platform::MakeChildWindowInfo(window_->native_handle(), layout.chrome);
    CefBrowserHost::CreateBrowser(
        window_info,
        big_picture_client_,
        nebula::ui::ResolveInternalUrl(nebula::ui::GetBigPictureUrl()),
        browser_settings,
        nullptr,
        nullptr);
}

void NebulaController::CreateContentBrowser() {
    if (!window_ || !window_->native_handle()) {
        return;
    }

    const auto* tab = tabs_.ActiveTab();
    const std::string url = tab && !tab->url.empty() ? tab->url : nebula::ui::GetHomeUrl();
    const auto layout = CurrentBrowserLayout();
    CefBrowserSettings browser_settings = BrowserSettings();
    content_client_ = new nebula::cef::NebulaBrowserClient(nebula::cef::BrowserRole::Content, this);
    CefWindowInfo window_info =
        nebula::platform::MakeChildWindowInfo(window_->native_handle(), layout.content);
    CefBrowserHost::CreateBrowser(
        window_info, content_client_, nebula::ui::ResolveInternalUrl(url), browser_settings, nullptr, nullptr);
}

void NebulaController::EnterBigPictureMode() {
    if (big_picture_mode_) {
        return;
    }

    CloseMenuPopup();
    if (content_fullscreen_) {
        SetContentFullscreen(false);
    }

    big_picture_mode_ = true;
    big_picture_browse_visible_ = false;
    if (auto* tab = tabs_.ActiveTab()) {
        InjectBigPictureCursor(tab->browser);
    }
    SetBrowserVisible(chrome_browser_, false);
    if (big_picture_browser_) {
        SetBrowserVisible(big_picture_browser_, true);
        if (const auto* tab = tabs_.ActiveTab()) {
            SendBigPictureState(*tab);
        }
    } else {
        CreateBigPictureBrowser();
    }
    ResizeBrowsers();
}

void NebulaController::ExitBigPictureMode() {
    if (!big_picture_mode_) {
        return;
    }

    big_picture_mode_ = false;
    big_picture_browse_visible_ = false;
    if (auto* tab = tabs_.ActiveTab()) {
        RemoveBigPictureCursor(tab->browser);
    }
    SetBrowserVisible(big_picture_browser_, false);
    SetBrowserVisible(chrome_browser_, true);
    if (const auto* tab = tabs_.ActiveTab()) {
        SendChromeState(*tab);
    }
    ResizeBrowsers();
}

nebula::window::BrowserLayout NebulaController::CurrentBrowserLayout() const {
    if (!window_) {
        return {};
    }

    if (first_run_setup_active_) {
        return window_->CurrentLayout(false);
    }

    if (!big_picture_mode_) {
        return window_->CurrentLayout(!content_fullscreen_);
    }

    const auto client_size = nebula::platform::ParentClientSize(window_->native_handle());
    if (!big_picture_browse_visible_) {
        nebula::window::BrowserLayout layout;
        layout.chrome = {0, 0, client_size.first, client_size.second};
        layout.content = {};
        return layout;
    }

    nebula::window::BrowserLayout layout;
    layout.chrome = {0, 0, client_size.first, client_size.second};

    // left_margin must clear the 220px sidebar; right_margin must clear the
    // native-browser-panel (clamp(168,18vw,260) + 20px right inset ≈ 280px max).
    // top/bottom margins must clear the header (~68px) and footer (~56px).
    const int left_margin  = nebula::platform::ScaleForParentWindow(window_->native_handle(), 224);
    const int right_margin = nebula::platform::ScaleForParentWindow(window_->native_handle(), 284);
    const int top_margin   = nebula::platform::ScaleForParentWindow(window_->native_handle(), 68);
    const int bottom_margin = nebula::platform::ScaleForParentWindow(window_->native_handle(), 56);
    const int available_width = std::max(0, client_size.first - left_margin - right_margin);
    const int available_height = std::max(0, client_size.second - top_margin - bottom_margin);

    layout.content = {
        left_margin,
        top_margin,
        available_width,
        available_height};
    return layout;
}

void NebulaController::ToggleMenuPopup() {
    if (big_picture_mode_) {
        return;
    }

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
    if (!window_ || !window_->native_handle() || content_fullscreen_ || big_picture_mode_) {
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

void NebulaController::SendThemeToChromeSurfaces(const std::string& theme_json) {
    if (theme_json.empty()) {
        return;
    }

    const std::string escaped_theme = nebula::browser::JsonEscape(theme_json);
    const std::string script =
        "(function(){"
        "try{"
        "const theme=JSON.parse(\"" + escaped_theme + "\");"
        "if(window.NebulaChrome&&window.NebulaChrome.applyTheme){window.NebulaChrome.applyTheme(theme);}"
        "if(window.NebulaMenuPopup&&window.NebulaMenuPopup.applyTheme){window.NebulaMenuPopup.applyTheme(theme);}"
        "}catch(e){console.warn('[Theme] Failed to apply chrome theme',e);}"
        "})();";

    if (chrome_browser_) {
        chrome_browser_->GetMainFrame()->ExecuteJavaScript(script, nebula::ui::GetChromeUrl(), 0);
    }
    if (menu_popup_browser_) {
        menu_popup_browser_->GetMainFrame()->ExecuteJavaScript(script, nebula::ui::GetMenuPopupUrl(), 0);
    }
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

void NebulaController::SendBigPictureMouseMove(const std::string& payload) {
    auto* tab = tabs_.ActiveTab();
    if (!tab || !tab->browser) {
        return;
    }

    int x = 0;
    int y = 0;
    if (!ParseTwoInts(payload, x, y)) {
        return;
    }

    CefMouseEvent event = {};
    event.x = x;
    event.y = y;
    nebula::platform::MoveCursorToBrowserPoint(tab->browser->GetHost()->GetWindowHandle(), x, y);
    tab->browser->GetHost()->SendMouseMoveEvent(event, false);
}

void NebulaController::SendBigPictureMouseClick(const std::string& payload, bool right_click) {
    auto* tab = tabs_.ActiveTab();
    if (!tab || !tab->browser) {
        return;
    }

    int x = 0;
    int y = 0;
    if (!ParseTwoInts(payload, x, y)) {
        return;
    }

    CefMouseEvent event = {};
    event.x = x;
    event.y = y;
    const auto button = right_click ? MBT_RIGHT : MBT_LEFT;
    nebula::platform::MoveCursorToBrowserPoint(tab->browser->GetHost()->GetWindowHandle(), x, y);
    tab->browser->GetHost()->SendMouseMoveEvent(event, false);
    tab->browser->GetHost()->SendMouseClickEvent(event, button, false, 1);
    tab->browser->GetHost()->SendMouseClickEvent(event, button, true, 1);
}

void NebulaController::SendBigPictureMouseWheel(const std::string& payload) {
    auto* tab = tabs_.ActiveTab();
    if (!tab || !tab->browser) {
        return;
    }

    int delta_x = 0;
    int delta_y = 0;
    int x = 0;
    int y = 0;
    const bool has_pointer = ParseFourInts(payload, delta_x, delta_y, x, y);
    if (!has_pointer && !ParseTwoInts(payload, delta_x, delta_y)) {
        return;
    }

    const auto layout = CurrentBrowserLayout();
    CefMouseEvent event = {};
    event.x = has_pointer ? std::clamp(x, 0, std::max(0, layout.content.width - 1))
                          : std::max(0, layout.content.width / 2);
    event.y = has_pointer ? std::clamp(y, 0, std::max(0, layout.content.height - 1))
                          : std::max(0, layout.content.height / 2);
    tab->browser->GetHost()->SendMouseWheelEvent(event, delta_x, delta_y);
}

void NebulaController::SendBigPictureText(const std::string& payload) {
    auto* tab = tabs_.ActiveTab();
    if (!tab || !tab->browser) {
        return;
    }

    const std::string script =
        "(function(){"
        "const el=document.activeElement;"
        "if(!el)return;"
        "const value=\"" + nebula::browser::JsonEscape(payload) + "\";"
        "const editable=el.tagName==='INPUT'||el.tagName==='TEXTAREA'||el.isContentEditable;"
        "if(!editable)return;"
        "if(el.isContentEditable){el.textContent=value;}else{el.value=value;}"
        "el.dispatchEvent(new Event('input',{bubbles:true}));"
        "el.dispatchEvent(new Event('change',{bubbles:true}));"
        "el.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',keyCode:13,bubbles:true}));"
        "el.dispatchEvent(new KeyboardEvent('keyup',{key:'Enter',keyCode:13,bubbles:true}));"
        "})();";
    tab->browser->GetMainFrame()->ExecuteJavaScript(script, tab->url, 0);
}

void NebulaController::SetBigPictureBrowseVisible(bool visible) {
    if (big_picture_browse_visible_ == visible) {
        return;
    }

    big_picture_browse_visible_ = visible;
    if (visible) {
        if (auto* tab = tabs_.ActiveTab()) {
            InjectBigPictureCursor(tab->browser);
        }
    }
    ResizeBrowsers();
    if (const auto* tab = tabs_.ActiveTab()) {
        SendBigPictureState(*tab);
    }
}

void NebulaController::SetContentFullscreen(bool fullscreen) {
    if (content_fullscreen_ == fullscreen) {
        return;
    }

    content_fullscreen_ = fullscreen;
    if (fullscreen) {
        CloseMenuPopup();
        ExitBigPictureMode();
    }

    SetBrowserVisible(chrome_browser_, !fullscreen);
    if (window_) {
        window_->SetFullscreen(fullscreen);
    }
    ResizeBrowsers();
}

void NebulaController::CompleteFirstRunSetup() {
    WriteFirstRunState(false);

    if (!first_run_setup_active_) {
        tabs_.LoadURL(nebula::ui::GetHomeUrl());
        PersistSession();
        return;
    }

    first_run_setup_active_ = false;
    tabs_.LoadURL(nebula::ui::GetHomeUrl());
    PersistSession();

    if (!big_picture_mode_ && !chrome_browser_) {
        CreateChromeBrowser();
    }
    ResizeBrowsers();
}

void NebulaController::ResizeBrowsers() {
    if (!window_) {
        return;
    }

    const auto layout = CurrentBrowserLayout();
    if (chrome_browser_) {
        window_->ResizeChild(
            chrome_browser_->GetHost()->GetWindowHandle(),
            layout.chrome);
    }
    if (big_picture_browser_) {
        window_->ResizeChild(
            big_picture_browser_->GetHost()->GetWindowHandle(),
            layout.chrome);
    }
    if (const auto* tab = tabs_.ActiveTab(); tab && tab->browser) {
        SetBrowserVisible(tab->browser, !big_picture_mode_ || big_picture_browse_visible_);
        window_->ResizeChild(
            tab->browser->GetHost()->GetWindowHandle(),
            layout.content);
    }
    if (!content_fullscreen_ && !big_picture_mode_) {
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

void NebulaController::SendBigPictureState(const nebula::browser::NebulaTab& tab) {
    if (!big_picture_browser_) {
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
            ",\"url\":\"" + nebula::browser::JsonEscape(item.url) + "\"" +
            ",\"isLoading\":" + std::string(item.is_loading ? "true" : "false") +
            ",\"favicon\":\"" + nebula::browser::JsonEscape(item.favicon_url) + "\"" +
            "}";
    }
    tabs_json += "]";

    std::string history_json = "[";
    for (size_t i = 0; i < site_history_.size(); ++i) {
        if (i > 0) {
            history_json += ",";
        }
        history_json += "\"" + nebula::browser::JsonEscape(site_history_[i]) + "\"";
    }
    history_json += "]";

    const auto layout = CurrentBrowserLayout();
    const std::string script =
        "window.NebulaBigPicture && window.NebulaBigPicture.applyState({"
        "\"id\":" + std::to_string(tab.id) +
        ",\"url\":\"" + nebula::browser::JsonEscape(display_url) + "\""
        ",\"title\":\"" + nebula::browser::JsonEscape(tab.title) + "\""
        ",\"isLoading\":" + std::string(tab.is_loading ? "true" : "false") +
        ",\"progress\":" + std::to_string(tab.load_progress) +
        ",\"canGoBack\":" + std::string(tab.CanGoBack() ? "true" : "false") +
        ",\"canGoForward\":" + std::string(tab.CanGoForward() ? "true" : "false") +
        ",\"favicon\":\"" + nebula::browser::JsonEscape(tab.favicon_url) + "\"" +
        ",\"zoomLevel\":" + std::to_string(zoom_level) +
        ",\"browserLayout\":{"
        "\"x\":" + std::to_string(layout.content.x) +
        ",\"y\":" + std::to_string(layout.content.y) +
        ",\"width\":" + std::to_string(layout.content.width) +
        ",\"height\":" + std::to_string(layout.content.height) +
        "}" +
        ",\"tabs\":" + tabs_json +
        ",\"history\":" + history_json +
        "});";

    big_picture_browser_->GetMainFrame()->ExecuteJavaScript(
        script,
        nebula::ui::GetBigPictureUrl(),
        0);
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

void NebulaController::InjectBigPictureCursor(CefRefPtr<CefBrowser> browser) {
    if (!big_picture_mode_ || !browser) {
        return;
    }

    static constexpr char kScript[] = R"JS(
(function(){
  const id = 'nebula-bigpicture-custom-cursor';
  const cursor = 'url("data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20width%3D%2232%22%20height%3D%2232%22%20viewBox%3D%220%200%2032%2032%22%3E%3Cpath%20d%3D%22M5%203v24l6.6-6.4%204.1%208.5%204.3-2.1-4.1-8.3H25L5%203z%22%20fill%3D%22%2300C6FF%22%20stroke%3D%22%23FFFFFF%22%20stroke-width%3D%222.2%22%20stroke-linejoin%3D%22round%22%2F%3E%3Cpath%20d%3D%22M9%207.7v9.8l2.5-2.4%202%204.1%201.5-.7-2-4h4L9%207.7z%22%20fill%3D%22%237B2EFF%22%20opacity%3D%220.8%22%2F%3E%3C%2Fsvg%3E") 5 3, auto';
  const css = 'html, body, body * { cursor: ' + cursor + ' !important; }';
  let style = document.getElementById(id);
  if (!style) {
    style = document.createElement('style');
    style.id = id;
    (document.head || document.documentElement).appendChild(style);
  }
  style.textContent = css;
})();
)JS";
    browser->GetMainFrame()->ExecuteJavaScript(kScript, browser->GetMainFrame()->GetURL(), 0);
}

void NebulaController::RemoveBigPictureCursor(CefRefPtr<CefBrowser> browser) {
    if (!browser) {
        return;
    }

    static constexpr char kScript[] = R"JS(
(function(){
  const style = document.getElementById('nebula-bigpicture-custom-cursor');
  if (style) {
    style.remove();
  }
})();
)JS";
    browser->GetMainFrame()->ExecuteJavaScript(kScript, browser->GetMainFrame()->GetURL(), 0);
}

void NebulaController::PersistSession() const {
    nebula::browser::SaveSessionState(tabs_.Tabs(), tabs_.ActiveTabIndex());
}

void NebulaController::MaybeFinishShutdown() {
    if (!closing_) {
        return;
    }

    if (chrome_browser_ || big_picture_browser_ || menu_popup_browser_ ||
        tabs_.HasOpenBrowsers() || !closing_tab_browsers_.empty()) {
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
