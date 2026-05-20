#include "app/first_run_state.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

#include "ui/paths.h"

namespace nebula::app {
namespace {

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool ReadFirstStartValue(const std::string& json, bool& first_start) {
    constexpr std::string_view key = "\"first-start\"";
    const size_t key_pos = json.find(key);
    if (key_pos == std::string::npos) {
        return false;
    }

    size_t colon = json.find(':', key_pos + key.size());
    if (colon == std::string::npos) {
        return false;
    }

    ++colon;
    while (colon < json.size() && std::isspace(static_cast<unsigned char>(json[colon]))) {
        ++colon;
    }

    if (json.compare(colon, 4, "true") == 0) {
        first_start = true;
        return true;
    }
    if (json.compare(colon, 5, "false") == 0) {
        first_start = false;
        return true;
    }

    return false;
}

}  // namespace

bool ShouldShowFirstRunSetup() {
    const auto path = nebula::ui::GetFirstRunStatePath();
    if (path.empty()) {
        return true;
    }

    const std::string json = ReadFile(path);
    if (json.empty()) {
        return true;
    }

    bool first_start = true;
    return ReadFirstStartValue(json, first_start) ? first_start : true;
}

bool WriteFirstRunState(bool first_start) {
    const auto path = nebula::ui::GetFirstRunStatePath();
    if (path.empty()) {
        return false;
    }

    std::filesystem::path temp_path = path;
    temp_path += L".tmp";
    {
        std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            return false;
        }
        output << "{\n  \"first-start\": " << (first_start ? "true" : "false") << "\n}\n";
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temp_path, path, ec);
    return !ec;
}

}  // namespace nebula::app
