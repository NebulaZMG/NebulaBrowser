#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "browser/tab_manager.h"
#include "cef/browser_client.h"
#include "window/nebula_window.h"

namespace nebula::app {

class NebulaController final : public nebula::window::WindowDelegate,
                              public nebula::browser::TabObserver,
                              public nebula::cef::BrowserClientDelegate {
public:
    NebulaController(HINSTANCE instance, std::string initial_url, int show_command);
    ~NebulaController() override;

    bool Create();

    void OnWindowCreated() override;
    void OnWindowResized(const nebula::window::BrowserLayout& layout) override;
    void OnWindowCloseRequested() override;

    void OnActiveTabChanged(const nebula::browser::NebulaTab& tab) override;

    void OnBrowserCreated(nebula::cef::BrowserRole role, CefRefPtr<CefBrowser> browser) override;
    void OnBrowserClosing(nebula::cef::BrowserRole role, CefRefPtr<CefBrowser> browser) override;
    void OnChromeCommand(const std::string& command, const std::string& payload) override;
    void OnContentAddressChanged(CefRefPtr<CefBrowser> browser, const std::string& url) override;
    void OnContentTitleChanged(CefRefPtr<CefBrowser> browser, const std::string& title) override;
    void OnContentLoadingStateChanged(CefRefPtr<CefBrowser> browser, bool is_loading) override;
    void OnContentLoadProgressChanged(CefRefPtr<CefBrowser> browser, double progress) override;
    void OnContentFaviconChanged(CefRefPtr<CefBrowser> browser, const std::vector<std::string>& urls) override;
    void OnPopupRequested(CefRefPtr<CefBrowser> browser, const std::string& target_url) override;
    bool ShouldBypassInsecureWarning(CefRefPtr<CefBrowser> browser, const std::string& target_url) override;

private:
    void CreateNewTab();
    void ActivateTab(int tab_id);
    void CloseTab(int tab_id);
    void CreateChromeBrowser();
    void CreateContentBrowser();
    void ToggleMenuPopup();
    void CloseMenuPopup();
    void CreateMenuPopupBrowser();
    void PositionMenuPopup();
    void ToggleDevTools();
    void AdjustZoom(double delta);
    void FreshReload();
    void ResizeBrowsers();
    void SendChromeState(const nebula::browser::NebulaTab& tab);
    void MaybeFinishShutdown();

    HINSTANCE instance_ = nullptr;
    std::string initial_url_;
    int show_command_ = SW_SHOWDEFAULT;
    bool closing_ = false;
    bool chrome_ready_ = false;

    std::unique_ptr<nebula::window::NebulaWindow> window_;
    nebula::browser::TabManager tabs_;
    CefRefPtr<CefBrowser> chrome_browser_;
    CefRefPtr<CefBrowser> menu_popup_browser_;
    CefRefPtr<nebula::cef::NebulaBrowserClient> chrome_client_;
    CefRefPtr<nebula::cef::NebulaBrowserClient> content_client_;
    CefRefPtr<nebula::cef::NebulaBrowserClient> menu_popup_client_;
    std::unordered_set<std::string> insecure_warning_bypasses_;
};

}  // namespace nebula::app
