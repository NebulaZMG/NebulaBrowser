#include "ui/paths.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <string_view>

namespace nebula::ui {
namespace {

constexpr std::string_view kNebulaScheme = "nebula://";
constexpr std::wstring_view kInternalFallbackPage = L"404.html";

struct InternalPage {
    std::string_view slug;
    std::wstring_view file_name;
};

constexpr InternalPage kInternalPages[] = {
    {"home", L"home.html"},
    {"settings", L"settings.html"},
    {"downloads", L"downloads.html"},
    {"bigpicture", L"bigpicture.html"},
    {"big-picture", L"bigpicture.html"},
    {"gpu-diagnostics", L"gpu-diagnostics.html"},
    {"setup", L"setup.html"},
    {"404", L"404.html"},
    {"insecure", L"insecure.html"},
};

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

std::string GetUrlDecoration(const std::string& url) {
    const size_t split = url.find_first_of("?#");
    return split == std::string::npos ? std::string{} : url.substr(split);
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string PageFileUrl(std::wstring_view page_name) {
    const auto path = GetUiPagePath(std::wstring(page_name));
    return path.empty() ? std::string{} : FilePathToUrl(path);
}

std::string PercentEncode(const std::string& value) {
    constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded += static_cast<char>(ch);
        } else {
            encoded += '%';
            encoded += kHex[ch >> 4];
            encoded += kHex[ch & 0x0F];
        }
    }
    return encoded;
}

std::string InternalPageName(const std::string& url) {
    std::string target = ToLowerAscii(GetUrlWithoutDecoration(url));
    if (!target.starts_with(kNebulaScheme)) {
        return {};
    }

    target.erase(0, kNebulaScheme.size());
    while (!target.empty() && target.front() == '/') {
        target.erase(target.begin());
    }
    while (!target.empty() && target.back() == '/') {
        target.pop_back();
    }
    return target.empty() ? "home" : target;
}

std::string InternalUrlForSlug(std::string_view slug) {
    return std::string(kNebulaScheme) + std::string(slug);
}

const InternalPage* FindInternalPageBySlug(std::string_view slug) {
    for (const auto& page : kInternalPages) {
        if (page.slug == slug) {
            return &page;
        }
    }
    return nullptr;
}

const InternalPage* FindInternalPageByFileUrl(const std::string& url) {
    const std::string base_url = GetUrlWithoutDecoration(url);
    for (const auto& page : kInternalPages) {
        if (PageFileUrl(page.file_name) == base_url) {
            return &page;
        }
    }
    return nullptr;
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

std::filesystem::path GetUserDataDirectory() {
    std::filesystem::path root;

    wchar_t buffer[MAX_PATH] = {};
    // Prefer %LOCALAPPDATA% so the profile follows Chromium conventions and
    // survives executable relocation.
    const DWORD length =
        GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        root = std::filesystem::path(buffer);
    } else {
        // Fall back to a directory next to the executable so a portable
        // install still gets a writable profile.
        root = GetExecutableDirectory();
    }

    if (root.empty()) {
        return {};
    }

    std::filesystem::path user_data = root / L"Nebula" / L"User Data";
    std::error_code ec;
    std::filesystem::create_directories(user_data, ec);
    return user_data;
}

std::filesystem::path GetCacheDirectory() {
    auto user_data = GetUserDataDirectory();
    if (user_data.empty()) {
        return {};
    }

    std::filesystem::path cache = user_data / L"Cache";
    std::error_code ec;
    std::filesystem::create_directories(cache, ec);
    return cache;
}

std::filesystem::path GetSessionStatePath() {
    auto user_data = GetUserDataDirectory();
    return user_data.empty() ? std::filesystem::path{} : user_data / L"session_state.json";
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
    const std::string fallback = PageFileUrl(L"home.html");
    return path.empty() ? (fallback.empty() ? "https://www.google.com" : fallback) : FilePathToUrl(path);
}

std::string GetHomeUrl() {
    return InternalUrlForSlug("home");
}

std::string GetSettingsUrl() {
    return InternalUrlForSlug("settings");
}

std::string GetDownloadsUrl() {
    return InternalUrlForSlug("downloads");
}

std::string GetBigPictureUrl() {
    return InternalUrlForSlug("bigpicture");
}

std::string GetGpuDiagnosticsUrl() {
    return InternalUrlForSlug("gpu-diagnostics");
}

std::string GetMenuPopupUrl() {
    const auto path = GetUiPagePath(L"menu-popup.html");
    const std::string fallback = PageFileUrl(L"home.html");
    return path.empty() ? (fallback.empty() ? "https://www.google.com" : fallback) : FilePathToUrl(path);
}

std::string GetInsecureWarningUrl(const std::string& target_url) {
    return InternalUrlForSlug("insecure") + "?target=" + PercentEncode(target_url);
}

std::string GetNotFoundUrl(const std::string& target_url) {
    return InternalUrlForSlug("404") + "?url=" + PercentEncode(target_url);
}

std::string ResolveInternalUrl(const std::string& url) {
    const std::string page_name = InternalPageName(url);
    if (page_name.empty()) {
        return url;
    }

    if (const auto* page = FindInternalPageBySlug(page_name)) {
        const std::string file_url = PageFileUrl(page->file_name);
        return file_url.empty() ? url : file_url + GetUrlDecoration(url);
    }

    const std::string fallback_url = PageFileUrl(kInternalFallbackPage);
    return fallback_url.empty() ? url : fallback_url + "?url=" + PercentEncode(url);
}

std::string ToInternalUrl(const std::string& url) {
    const std::string page_name = InternalPageName(url);
    if (!page_name.empty()) {
        if (const auto* page = FindInternalPageBySlug(page_name)) {
            return InternalUrlForSlug(page->slug) + GetUrlDecoration(url);
        }
        return url;
    }

    if (const auto* page = FindInternalPageByFileUrl(url)) {
        return InternalUrlForSlug(page->slug) + GetUrlDecoration(url);
    }

    return url;
}

bool IsInternalHomeUrl(const std::string& url) {
    return GetUrlWithoutDecoration(ToInternalUrl(url)) == GetHomeUrl();
}

bool IsNebulaInternalUrl(const std::string& url) {
    return ToLowerAscii(url).starts_with(kNebulaScheme);
}

bool IsHttpUrl(const std::string& url) {
    return ToLowerAscii(url).starts_with("http://");
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
