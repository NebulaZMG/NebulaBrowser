#include "ui/paths.h"

#include <windows.h>

#include <algorithm>
#include <cctype>

namespace nebula::ui {
namespace {

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::string GetUrlWithoutDecoration(std::string url) {
    const size_t split = url.find_first_of("?#");
    if (split != std::string::npos) {
        url.resize(split);
    }
    return url;
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

}  // namespace

std::filesystem::path GetExecutableDirectory() {
    wchar_t exe_path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return {};
    }

    return std::filesystem::path(exe_path).parent_path();
}

std::filesystem::path GetUiPagePath(const std::wstring& page_name) {
    const auto exe_dir = GetExecutableDirectory();
    if (exe_dir.empty()) {
        return {};
    }

    return exe_dir / L"ui" / L"pages" / page_name;
}

std::string FilePathToUrl(std::filesystem::path path) {
    std::string value = WideToUtf8(path.wstring());
    for (char& ch : value) {
        if (ch == '\\') {
            ch = '/';
        }
    }

    std::string encoded;
    encoded.reserve(value.size());
    for (char ch : value) {
        encoded += ch == ' ' ? "%20" : std::string(1, ch);
    }
    return "file:///" + encoded;
}

std::string GetChromeUrl() {
    const auto path = GetUiPagePath(L"chrome.html");
    return path.empty() ? GetHomeUrl() : FilePathToUrl(path);
}

std::string GetHomeUrl() {
    const auto path = GetUiPagePath(L"home.html");
    return path.empty() ? "https://www.google.com" : FilePathToUrl(path);
}

std::string GetSettingsUrl() {
    const auto path = GetUiPagePath(L"settings.html");
    return path.empty() ? GetHomeUrl() : FilePathToUrl(path);
}

std::string GetBigPictureUrl() {
    const auto path = GetUiPagePath(L"bigpicture.html");
    return path.empty() ? GetHomeUrl() : FilePathToUrl(path);
}

std::string GetMenuPopupUrl() {
    const auto path = GetUiPagePath(L"menu-popup.html");
    return path.empty() ? GetHomeUrl() : FilePathToUrl(path);
}

bool IsInternalHomeUrl(const std::string& url) {
    return GetUrlWithoutDecoration(url) == GetHomeUrl();
}

bool IsChromiumNewTabUrl(const std::string& url) {
    const std::string target = ToLowerAscii(GetUrlWithoutDecoration(url));
    return target == "about:blank" ||
           target == "chrome://newtab" ||
           target == "chrome://newtab/" ||
           target == "chrome://new-tab-page" ||
           target == "chrome://new-tab-page/" ||
           target == "chrome-search://local-ntp/local-ntp.html";
}

bool IsEmptyOrChromiumNewTabUrl(const std::string& url) {
    return url.empty() || IsChromiumNewTabUrl(url);
}

}  // namespace nebula::ui
