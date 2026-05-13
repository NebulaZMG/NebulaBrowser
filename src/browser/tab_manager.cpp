#include "browser/tab_manager.h"

#include "browser/url_utils.h"

namespace nebula::browser {

TabManager::TabManager(TabObserver* observer) : observer_(observer) {}

NebulaTab& TabManager::CreateInitialTab(std::string initial_url) {
    tabs_.clear();
    NebulaTab tab;
    tab.id = next_tab_id_++;
    tab.url = std::move(initial_url);
    tabs_.push_back(std::move(tab));
    active_tab_id_ = tabs_.front().id;
    Notify();
    return tabs_.front();
}

NebulaTab& TabManager::CreateTab(std::string url) {
    NebulaTab tab;
    tab.id = next_tab_id_++;
    tab.url = std::move(url);
    tabs_.push_back(std::move(tab));
    active_tab_id_ = tabs_.back().id;
    Notify();
    return tabs_.back();
}

NebulaTab* TabManager::ActiveTab() {
    for (auto& tab : tabs_) {
        if (tab.id == active_tab_id_) {
            return &tab;
        }
    }
    return nullptr;
}

const NebulaTab* TabManager::ActiveTab() const {
    for (const auto& tab : tabs_) {
        if (tab.id == active_tab_id_) {
            return &tab;
        }
    }
    return nullptr;
}

const std::vector<NebulaTab>& TabManager::Tabs() const {
    return tabs_;
}

bool TabManager::ActivateTab(int tab_id) {
    if (!FindTab(tab_id)) {
        return false;
    }

    active_tab_id_ = tab_id;
    Notify();
    return true;
}

CefRefPtr<CefBrowser> TabManager::CloseTab(int tab_id) {
    for (auto it = tabs_.begin(); it != tabs_.end(); ++it) {
        if (it->id != tab_id) {
            continue;
        }

        CefRefPtr<CefBrowser> browser = it->browser;
        const bool was_active = it->id == active_tab_id_;
        const auto next_it = tabs_.erase(it);

        if (tabs_.empty()) {
            active_tab_id_ = 0;
        } else if (was_active) {
            active_tab_id_ = next_it != tabs_.end() ? next_it->id : tabs_.back().id;
        }

        Notify();
        return browser;
    }

    return nullptr;
}

void TabManager::SetActiveBrowser(CefRefPtr<CefBrowser> browser) {
    if (NebulaTab* tab = ActiveTab()) {
        tab->browser = browser;
        if (browser && tab->url.empty()) {
            tab->url = browser->GetMainFrame()->GetURL();
        }
        Notify();
    }
}

bool TabManager::OwnsBrowser(CefRefPtr<CefBrowser> browser) const {
    if (!browser) {
        return false;
    }

    for (const auto& tab : tabs_) {
        if (tab.browser && tab.browser->IsSame(browser)) {
            return true;
        }
    }
    return false;
}

bool TabManager::HasOpenBrowsers() const {
    for (const auto& tab : tabs_) {
        if (tab.browser) {
            return true;
        }
    }
    return false;
}

void TabManager::ClearBrowser(CefRefPtr<CefBrowser> browser) {
    if (NebulaTab* tab = FindTab(browser)) {
        tab->browser = nullptr;
        Notify();
    }
}

void TabManager::LoadURL(const std::string& input) {
    NebulaTab* tab = ActiveTab();
    if (!tab || !tab->browser) {
        return;
    }

    const std::string target = NormalizeNavigationInput(input);
    if (target.empty()) {
        return;
    }

    tab->url = target;
    tab->favicon_url.clear();
    tab->browser->GetMainFrame()->LoadURL(target);
    Notify();
}

void TabManager::GoBack() {
    NebulaTab* tab = ActiveTab();
    if (tab && tab->browser && tab->browser->CanGoBack()) {
        tab->browser->GoBack();
    }
}

void TabManager::GoForward() {
    NebulaTab* tab = ActiveTab();
    if (tab && tab->browser && tab->browser->CanGoForward()) {
        tab->browser->GoForward();
    }
}

void TabManager::Reload() {
    NebulaTab* tab = ActiveTab();
    if (tab && tab->browser) {
        tab->browser->Reload();
    }
}

void TabManager::StopLoad() {
    NebulaTab* tab = ActiveTab();
    if (tab && tab->browser) {
        tab->browser->StopLoad();
    }
}

void TabManager::UpdateURL(CefRefPtr<CefBrowser> browser, std::string url) {
    if (NebulaTab* tab = FindTab(browser)) {
        tab->url = std::move(url);
        Notify();
    }
}

void TabManager::UpdateTitle(CefRefPtr<CefBrowser> browser, std::string title) {
    if (NebulaTab* tab = FindTab(browser)) {
        tab->title = title.empty() ? "New Tab" : std::move(title);
        Notify();
    }
}

void TabManager::UpdateLoadingState(CefRefPtr<CefBrowser> browser, bool is_loading) {
    if (NebulaTab* tab = FindTab(browser)) {
        tab->is_loading = is_loading;
        if (is_loading) {
            tab->favicon_url.clear();
        }
        if (!is_loading) {
            tab->load_progress = 1.0;
        }
        Notify();
    }
}

void TabManager::UpdateLoadProgress(CefRefPtr<CefBrowser> browser, double progress) {
    if (NebulaTab* tab = FindTab(browser)) {
        tab->load_progress = progress;
        Notify();
    }
}

void TabManager::UpdateFavicon(CefRefPtr<CefBrowser> browser, const std::vector<std::string>& urls) {
    if (NebulaTab* tab = FindTab(browser)) {
        tab->favicon_url = urls.empty() ? std::string{} : urls.front();
        Notify();
    }
}

void TabManager::Notify() {
    const NebulaTab* tab = ActiveTab();
    if (observer_ && tab) {
        observer_->OnActiveTabChanged(*tab);
    }
}

NebulaTab* TabManager::FindTab(int tab_id) {
    for (auto& tab : tabs_) {
        if (tab.id == tab_id) {
            return &tab;
        }
    }
    return nullptr;
}

NebulaTab* TabManager::FindTab(CefRefPtr<CefBrowser> browser) {
    if (!browser) {
        return nullptr;
    }

    for (auto& tab : tabs_) {
        if (tab.browser && tab.browser->IsSame(browser)) {
            return &tab;
        }
    }
    return nullptr;
}

}  // namespace nebula::browser
