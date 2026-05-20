#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "app/run.h"
#include "browser/tab_manager.h"
#include "cef/browser_client.h"
#include "platform/types.h"
#include "window/nebula_window.h"

namespace nebula::app {

class NebulaController final : public nebula::window::WindowDelegate,
                              public nebula::browser::TabObserver,
                              public nebula::cef::BrowserClientDelegate {
public:
    NebulaController(nebula::platform::AppStartup startup,
                     std::string initial_url,
                     LaunchOptions launch_options = {});
    ~NebulaController() override;

    bool Create();

    void OnWindowCreated() override;
    void OnWindowResized(const nebula::window::BrowserLayout& layout) override;
    void OnWindowCloseRequested() override;
    void OnExternalOpenRequested(const std::string& target) override;

    void OnActiveTabChanged(const nebula::browser::NebulaTab& tab) override;

    void OnBrowserCreated(nebula::cef::BrowserRole role, CefRefPtr<CefBrowser> browser) override;
    void OnBrowserClosing(nebula::cef::BrowserRole role, CefRefPtr<CefBrowser> browser) override;
    void OnChromeCommand(const std::string& command, const std::string& payload) override;
    void OnContentAddressChanged(CefRefPtr<CefBrowser> browser, const std::string& url) override;
    void OnContentTitleChanged(CefRefPtr<CefBrowser> browser, const std::string& title) override;
    void OnContentLoadingStateChanged(CefRefPtr<CefBrowser> browser, bool is_loading) override;
    void OnContentLoadProgressChanged(CefRefPtr<CefBrowser> browser, double progress) override;
    void OnContentLoadFinished(CefRefPtr<CefBrowser> browser, const std::string& url) override;
    void OnContentFaviconChanged(CefRefPtr<CefBrowser> browser, const std::vector<std::string>& urls) override;
    void OnContentFullscreenChanged(CefRefPtr<CefBrowser> browser, bool fullscreen) override;
    void OnPopupRequested(CefRefPtr<CefBrowser> browser, const std::string& target_url) override;
    bool ShouldBypassInsecureWarning(CefRefPtr<CefBrowser> browser, const std::string& target_url) override;

private:
    void CreateNewTab(std::string url = {});
    void ActivateTab(int tab_id);
    void CloseTab(int tab_id);
    void CreateChromeBrowser();
    void CreateBigPictureBrowser();
    void CreateContentBrowser();
    void EnterBigPictureMode();
    void ExitBigPictureMode();
    nebula::window::BrowserLayout CurrentBrowserLayout() const;
    void ToggleMenuPopup();
    void CloseMenuPopup();
    void CreateMenuPopupBrowser();
    void PositionMenuPopup();
    void SendMenuPopupZoom();
    void SendThemeToChromeSurfaces(const std::string& theme_json);
    void ToggleDevTools();
    void AdjustZoom(double delta);
    void FreshReload();
    void SendBigPictureMouseMove(const std::string& payload);
    void SendBigPictureMouseClick(const std::string& payload, bool right_click);
    void SendBigPictureMouseWheel(const std::string& payload);
    void SendBigPictureText(const std::string& payload);
    void SetBigPictureBrowseVisible(bool visible);
    void SetContentFullscreen(bool fullscreen);
    void CompleteFirstRunSetup();
    void SendDefaultBrowserResult(const std::string& request_id,
                                  bool success,
                                  bool needs_user_action,
                                  const std::string& error = {});
    void ResizeBrowsers();
    void SendChromeState(const nebula::browser::NebulaTab& tab);
    void SendBigPictureState(const nebula::browser::NebulaTab& tab);
    void RecordSiteHistory(const std::string& url);
    void InjectSettingsHistory(CefRefPtr<CefBrowser> browser);
    void InjectBigPictureCursor(CefRefPtr<CefBrowser> browser);
    void RemoveBigPictureCursor(CefRefPtr<CefBrowser> browser);
    void PersistSession() const;
    void BeginShutdown();
    void MaybeFinishShutdown();
    bool ForgetClosingTabBrowser(CefRefPtr<CefBrowser> browser);
    void LoadPendingNavigationDelayed();

    nebula::platform::AppStartup startup_;
    std::string initial_url_;
    std::string pending_initial_navigation_;
    LaunchOptions launch_options_;
    bool closing_ = false;
    bool chrome_ready_ = false;
    bool big_picture_ready_ = false;
    bool big_picture_mode_ = false;
    bool big_picture_browse_visible_ = false;
    bool content_fullscreen_ = false;
    bool first_run_setup_active_ = false;
    bool menu_popup_visible_ = false;

    std::unique_ptr<nebula::window::NebulaWindow> window_;
    nebula::browser::TabManager tabs_;
    CefRefPtr<CefBrowser> chrome_browser_;
    CefRefPtr<CefBrowser> big_picture_browser_;
    CefRefPtr<CefBrowser> menu_popup_browser_;
    CefRefPtr<nebula::cef::NebulaBrowserClient> chrome_client_;
    CefRefPtr<nebula::cef::NebulaBrowserClient> big_picture_client_;
    CefRefPtr<nebula::cef::NebulaBrowserClient> content_client_;
    CefRefPtr<nebula::cef::NebulaBrowserClient> menu_popup_client_;
    std::vector<CefRefPtr<CefBrowser>> closing_tab_browsers_;
    std::unordered_set<std::string> insecure_warning_bypasses_;
    std::vector<std::string> site_history_;
};

}  // namespace nebula::app
