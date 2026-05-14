#include "browser/session_state.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>

#include "browser/url_utils.h"
#include "ui/paths.h"

namespace nebula::browser {
namespace {

constexpr size_t kMaxRestoredTabs = 50;

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::optional<size_t> ReadUnsignedValue(const std::string& json, std::string_view key) {
    const size_t key_pos = json.find(key);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    size_t colon = json.find(':', key_pos + key.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }

    ++colon;
    while (colon < json.size() && std::isspace(static_cast<unsigned char>(json[colon]))) {
        ++colon;
    }

    size_t end = colon;
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) {
        ++end;
    }

    size_t value = 0;
    const auto result = std::from_chars(json.data() + colon, json.data() + end, value);
    if (result.ec != std::errc{} || result.ptr != json.data() + end) {
        return std::nullopt;
    }

    return value;
}

std::optional<std::string> ReadStringValue(const std::string& object, std::string_view key) {
    const size_t key_pos = object.find(key);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    size_t colon = object.find(':', key_pos + key.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }

    size_t quote = object.find('"', colon + 1);
    if (quote == std::string::npos) {
        return std::nullopt;
    }

    std::string value;
    for (size_t i = quote + 1; i < object.size(); ++i) {
        const char ch = object[i];
        if (ch == '"') {
            return value;
        }

        if (ch != '\\') {
            value += ch;
            continue;
        }

        if (++i >= object.size()) {
            return std::nullopt;
        }

        switch (object[i]) {
            case '"':
            case '\\':
            case '/':
                value += object[i];
                break;
            case 'b':
                value += '\b';
                break;
            case 'f':
                value += '\f';
                break;
            case 'n':
                value += '\n';
                break;
            case 'r':
                value += '\r';
                break;
            case 't':
                value += '\t';
                break;
            default:
                return std::nullopt;
        }
    }

    return std::nullopt;
}

std::vector<PersistedTab> ReadTabs(const std::string& json) {
    std::vector<PersistedTab> tabs;
    const size_t tabs_pos = json.find("\"tabs\"");
    if (tabs_pos == std::string::npos) {
        return tabs;
    }

    const size_t array_start = json.find('[', tabs_pos);
    const size_t array_end = json.find(']', array_start == std::string::npos ? tabs_pos : array_start);
    if (array_start == std::string::npos || array_end == std::string::npos) {
        return tabs;
    }

    size_t cursor = array_start + 1;
    while (cursor < array_end && tabs.size() < kMaxRestoredTabs) {
        const size_t object_start = json.find('{', cursor);
        if (object_start == std::string::npos || object_start >= array_end) {
            break;
        }

        const size_t object_end = json.find('}', object_start + 1);
        if (object_end == std::string::npos || object_end > array_end) {
            break;
        }

        const std::string object = json.substr(object_start, object_end - object_start + 1);
        const auto url = ReadStringValue(object, "\"url\"");
        if (url && !url->empty()) {
            PersistedTab tab;
            tab.url = *url;
            if (const auto title = ReadStringValue(object, "\"title\""); title && !title->empty()) {
                tab.title = *title;
            }
            tabs.push_back(std::move(tab));
        }

        cursor = object_end + 1;
    }

    return tabs;
}

}  // namespace

SessionState LoadSessionState() {
    SessionState state;
    const std::string json = ReadFile(nebula::ui::GetSessionStatePath());
    if (json.empty()) {
        return state;
    }

    state.tabs = ReadTabs(json);
    if (const auto active_index = ReadUnsignedValue(json, "\"activeTabIndex\"")) {
        state.active_tab_index = *active_index;
    }

    if (!state.tabs.empty()) {
        state.active_tab_index = std::min(state.active_tab_index, state.tabs.size() - 1);
    } else {
        state.active_tab_index = 0;
    }

    return state;
}

void SaveSessionState(const std::vector<NebulaTab>& tabs, size_t active_tab_index) {
    const auto path = nebula::ui::GetSessionStatePath();
    if (path.empty()) {
        return;
    }

    std::ostringstream json;
    json << "{\n  \"activeTabIndex\": " << active_tab_index << ",\n  \"tabs\": [\n";

    bool wrote_tab = false;
    for (const auto& tab : tabs) {
        if (tab.url.empty()) {
            continue;
        }

        if (wrote_tab) {
            json << ",\n";
        }

        json << "    {\"url\": \"" << JsonEscape(tab.url)
             << "\", \"title\": \"" << JsonEscape(tab.title) << "\"}";
        wrote_tab = true;
    }

    json << "\n  ]\n}\n";

    std::filesystem::path temp_path = path;
    temp_path += L".tmp";
    {
        std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            return;
        }
        output << json.str();
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temp_path, path, ec);
}

}  // namespace nebula::browser
