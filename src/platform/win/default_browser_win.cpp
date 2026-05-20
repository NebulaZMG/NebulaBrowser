#include "platform/default_browser.h"

#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace nebula::platform {
namespace {

constexpr wchar_t kAppName[] = L"Nebula Browser";
constexpr wchar_t kClientKeyName[] = L"NebulaBrowser";
constexpr wchar_t kRegisteredApplicationsKey[] = L"Software\\RegisteredApplications";
constexpr wchar_t kClientRootKey[] = L"Software\\Clients\\StartMenuInternet\\NebulaBrowser";
constexpr wchar_t kFileAssociationsKey[] =
    L"Software\\Clients\\StartMenuInternet\\NebulaBrowser\\Capabilities\\FileAssociations";
constexpr wchar_t kUrlAssociationsKey[] =
    L"Software\\Clients\\StartMenuInternet\\NebulaBrowser\\Capabilities\\URLAssociations";
constexpr wchar_t kHttpUserChoiceKey[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice";
constexpr wchar_t kHttpsUserChoiceKey[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\https\\UserChoice";
constexpr std::wstring_view kWebFileExtensions[] = {
    L".htm",
    L".html",
    L".shtml",
    L".xht",
    L".xhtml",
    L".svg",
    L".webp",
};

std::wstring CurrentExecutablePath() {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = 0;
    while (true) {
        length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) {
            return {};
        }
        if (length < path.size() - 1) {
            path.resize(length);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

std::wstring Quote(std::wstring_view value) {
    std::wstring quoted = L"\"";
    quoted += value;
    quoted += L"\"";
    return quoted;
}

bool SetStringValue(HKEY root,
                    const std::wstring& subkey,
                    const wchar_t* value_name,
                    const std::wstring& value) {
    HKEY key = nullptr;
    const LSTATUS status = RegCreateKeyExW(
        root, subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) {
        return false;
    }

    const DWORD byte_size = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    const LSTATUS set_status = RegSetValueExW(
        key,
        value_name,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        byte_size);
    RegCloseKey(key);
    return set_status == ERROR_SUCCESS;
}

std::wstring ReadStringValue(HKEY root, const wchar_t* subkey, const wchar_t* value_name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return {};
    }

    DWORD type = 0;
    DWORD byte_size = 0;
    if (RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &byte_size) != ERROR_SUCCESS ||
        type != REG_SZ || byte_size == 0) {
        RegCloseKey(key);
        return {};
    }

    std::wstring value(byte_size / sizeof(wchar_t), L'\0');
    const LSTATUS status = RegQueryValueExW(
        key, value_name, nullptr, nullptr, reinterpret_cast<BYTE*>(value.data()), &byte_size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return {};
    }

    while (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return value;
}

bool RegisterDefaultBrowserCapabilities() {
    const std::wstring exe_path = CurrentExecutablePath();
    if (exe_path.empty()) {
        return false;
    }

    const std::wstring exe_name = std::filesystem::path(exe_path).filename().wstring();
    const std::wstring command = Quote(exe_path) + L" --url=" + Quote(L"%1");
    bool ok = true;

    ok &= SetStringValue(HKEY_CURRENT_USER, kRegisteredApplicationsKey, kAppName,
                         std::wstring(kClientRootKey) + L"\\Capabilities");
    ok &= SetStringValue(HKEY_CURRENT_USER, kClientRootKey, nullptr, kAppName);
    ok &= SetStringValue(HKEY_CURRENT_USER, std::wstring(kClientRootKey) + L"\\DefaultIcon",
                         nullptr, exe_path + L",0");
    ok &= SetStringValue(HKEY_CURRENT_USER, std::wstring(kClientRootKey) + L"\\shell\\open\\command",
                         nullptr, Quote(exe_path));
    ok &= SetStringValue(HKEY_CURRENT_USER, std::wstring(kClientRootKey) + L"\\Capabilities",
                         L"ApplicationName", kAppName);
    ok &= SetStringValue(HKEY_CURRENT_USER, std::wstring(kClientRootKey) + L"\\Capabilities",
                         L"ApplicationDescription", L"Nebula Browser");
    for (std::wstring_view extension : kWebFileExtensions) {
        const std::wstring extension_name(extension);
        ok &= SetStringValue(HKEY_CURRENT_USER, kFileAssociationsKey, extension_name.c_str(),
                             kClientKeyName);
    }
    ok &= SetStringValue(HKEY_CURRENT_USER, kUrlAssociationsKey, L"http", kClientKeyName);
    ok &= SetStringValue(HKEY_CURRENT_USER, kUrlAssociationsKey, L"https", kClientKeyName);
    ok &= SetStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\NebulaBrowser", nullptr,
                         L"Nebula Browser HTML Document");
    ok &= SetStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\NebulaBrowser", L"URL Protocol",
                         L"");
    ok &= SetStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\NebulaBrowser\\DefaultIcon",
                         nullptr, exe_path + L",0");
    ok &= SetStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\NebulaBrowser\\shell\\open\\command",
                         nullptr, command);
    const std::wstring application_key = std::wstring(L"Software\\Classes\\Applications\\") + exe_name;
    ok &= SetStringValue(HKEY_CURRENT_USER, application_key,
                         L"ApplicationName", kAppName);
    ok &= SetStringValue(HKEY_CURRENT_USER,
                         application_key + L"\\shell\\open\\command",
                         nullptr, command);

    return ok;
}

void OpenDefaultAppsSettings() {
    ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace

bool IsDefaultBrowser() {
    return ReadStringValue(HKEY_CURRENT_USER, kHttpUserChoiceKey, L"ProgId") == kClientKeyName &&
           ReadStringValue(HKEY_CURRENT_USER, kHttpsUserChoiceKey, L"ProgId") == kClientKeyName;
}

bool EnsureDefaultBrowserRegistration() {
    return RegisterDefaultBrowserCapabilities();
}

bool RequestDefaultBrowser() {
    const bool registered = RegisterDefaultBrowserCapabilities();
    OpenDefaultAppsSettings();
    return registered;
}

}  // namespace nebula::platform
