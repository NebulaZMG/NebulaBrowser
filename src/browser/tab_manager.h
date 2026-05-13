#pragma once

#include <string>
#include <vector>

#include "browser/tab.h"

namespace nebula::browser {

class TabObserver {
public:
    virtual ~TabObserver() = default;
    virtual void OnActiveTabChanged(const NebulaTab& tab) = 0;
};

class TabManager {
public:
    explicit TabManager(TabObserver* observer);

    NebulaTab& CreateInitialTab(std::string initial_url);
    NebulaTab& CreateTab(std::string url);
    NebulaTab* ActiveTab();
    const NebulaTab* ActiveTab() const;
    const std::vector<NebulaTab>& Tabs() const;

    bool ActivateTab(int tab_id);
    CefRefPtr<CefBrowser> CloseTab(int tab_id);
    void SetActiveBrowser(CefRefPtr<CefBrowser> browser);
    bool OwnsBrowser(CefRefPtr<CefBrowser> browser) const;
    void ClearBrowser(CefRefPtr<CefBrowser> browser);
    bool HasOpenBrowsers() const;

    void LoadURL(const std::string& input);
    void GoBack();
    void GoForward();
    void Reload();
    void StopLoad();

    void UpdateURL(CefRefPtr<CefBrowser> browser, std::string url);
    void UpdateTitle(CefRefPtr<CefBrowser> browser, std::string title);
    void UpdateLoadingState(CefRefPtr<CefBrowser> browser, bool is_loading);
    void UpdateLoadProgress(CefRefPtr<CefBrowser> browser, double progress);
    void UpdateFavicon(CefRefPtr<CefBrowser> browser, const std::vector<std::string>& urls);

private:
    void Notify();
    NebulaTab* FindTab(int tab_id);
    NebulaTab* FindTab(CefRefPtr<CefBrowser> browser);

    TabObserver* observer_ = nullptr;
    std::vector<NebulaTab> tabs_;
    int active_tab_id_ = 0;
    int next_tab_id_ = 1;
};

}  // namespace nebula::browser
