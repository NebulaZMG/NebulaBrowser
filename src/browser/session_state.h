#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "browser/tab.h"

namespace nebula::browser {

struct PersistedTab {
    std::string url;
    std::string title = "New Tab";
};

struct SessionState {
    std::vector<PersistedTab> tabs;
    size_t active_tab_index = 0;
};

SessionState LoadSessionState();
void SaveSessionState(const std::vector<NebulaTab>& tabs, size_t active_tab_index);

}  // namespace nebula::browser
