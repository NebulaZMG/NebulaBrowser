#include <windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"
#include "include/cef_request.h"
#include "include/cef_request_handler.h"
#include "include/wrapper/cef_helpers.h"

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

std::filesystem::path GetHomePath() {
    wchar_t exe_path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return {};
    }

    return std::filesystem::path(exe_path).parent_path() /
           "ui" / "pages" / "home.html";
}

std::string GetHomeUrl() {
    const auto home_path = GetHomePath();
    if (home_path.empty()) {
        return "https://www.google.com";
    }

    return FilePathToUrl(home_path);
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

class NebulaClient final : public CefClient,
                           public CefDisplayHandler,
                           public CefKeyboardHandler,
                           public CefLifeSpanHandler,
                           public CefPermissionHandler,
                           public CefRequestHandler {
public:
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }

    CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override {
        return this;
    }

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }

    CefRefPtr<CefPermissionHandler> GetPermissionHandler() override {
        return this;
    }

    CefRefPtr<CefRequestHandler> GetRequestHandler() override {
        return this;
    }

    void OnAddressChange(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         const CefString& url) override {
        CEF_REQUIRE_UI_THREAD();

        if (browser && frame && frame->IsMain() &&
            IsChromiumNewTabUrl(url)) {
            browser->GetMainFrame()->LoadURL(GetHomeUrl());
        }
    }

    void OnTitleChange(CefRefPtr<CefBrowser> browser,
                       const CefString& title) override {
        CEF_REQUIRE_UI_THREAD();

        CefWindowHandle window = browser->GetHost()->GetWindowHandle();
        if (window) {
            SetWindowText(window, std::wstring(title).c_str());
        }
    }

    bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                       const CefKeyEvent& event,
                       CefEventHandle os_event,
                       bool* is_keyboard_shortcut) override {
        CEF_REQUIRE_UI_THREAD();
        UNREFERENCED_PARAMETER(os_event);

        if (event.type == KEYEVENT_RAWKEYDOWN &&
            (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0 &&
            event.windows_key_code == 'T') {
            if (is_keyboard_shortcut) {
                *is_keyboard_shortcut = true;
            }
            browser->GetMainFrame()->LoadURL(GetHomeUrl());
            return true;
        }

        return false;
    }

    bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int popup_id,
                       const CefString& target_url,
                       const CefString& target_frame_name,
                       CefLifeSpanHandler::WindowOpenDisposition target_disposition,
                       bool user_gesture,
                       const CefPopupFeatures& popupFeatures,
                       CefWindowInfo& windowInfo,
                       CefRefPtr<CefClient>& client,
                       CefBrowserSettings& settings,
                       CefRefPtr<CefDictionaryValue>& extra_info,
                       bool* no_javascript_access) override {
        CEF_REQUIRE_UI_THREAD();
        UNREFERENCED_PARAMETER(frame);
        UNREFERENCED_PARAMETER(popup_id);
        UNREFERENCED_PARAMETER(target_frame_name);
        UNREFERENCED_PARAMETER(user_gesture);
        UNREFERENCED_PARAMETER(popupFeatures);
        UNREFERENCED_PARAMETER(windowInfo);
        UNREFERENCED_PARAMETER(settings);
        UNREFERENCED_PARAMETER(extra_info);
        UNREFERENCED_PARAMETER(no_javascript_access);

        if (target_disposition == CEF_WOD_NEW_WINDOW &&
            IsEmptyOrChromiumNewTabUrl(target_url)) {
            client = this;
            return false;
        }

        if (IsChromiumNewTabUrl(target_url)) {
            browser->GetMainFrame()->LoadURL(GetHomeUrl());
            return true;
        }

        return false;
    }

    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request,
                        bool user_gesture,
                        bool is_redirect) override {
        CEF_REQUIRE_UI_THREAD();
        UNREFERENCED_PARAMETER(browser);
        UNREFERENCED_PARAMETER(user_gesture);
        UNREFERENCED_PARAMETER(is_redirect);

        if (frame && frame->IsMain() && request &&
            IsChromiumNewTabUrl(request->GetURL())) {
            frame->LoadURL(GetHomeUrl());
            return true;
        }

        return false;
    }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        ++browser_count_;

        if (browser_count_ > 1 && browser &&
            IsEmptyOrChromiumNewTabUrl(browser->GetMainFrame()->GetURL())) {
            browser->GetMainFrame()->LoadURL(GetHomeUrl());
        }
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();

        --browser_count_;
        if (browser_count_ == 0) {
            CefQuitMessageLoop();
        }
    }

    bool OnShowPermissionPrompt(
        CefRefPtr<CefBrowser> browser,
        uint64_t prompt_id,
        const CefString& requesting_origin,
        uint32_t requested_permissions,
        CefRefPtr<CefPermissionPromptCallback> callback) override {
        CEF_REQUIRE_UI_THREAD();
        UNREFERENCED_PARAMETER(prompt_id);
        UNREFERENCED_PARAMETER(requesting_origin);

        if ((requested_permissions & CEF_PERMISSION_TYPE_GEOLOCATION) != 0 &&
            browser && callback &&
            IsInternalHomeUrl(browser->GetMainFrame()->GetURL())) {
            callback->Continue(CEF_PERMISSION_RESULT_ACCEPT);
            return true;
        }

        return false;
    }

private:
    int browser_count_ = 0;

    IMPLEMENT_REFCOUNTING(NebulaClient);
};

class NebulaApp final : public CefApp {
public:
    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override {
        UNREFERENCED_PARAMETER(process_type);

        // The bundled UI is loaded from file:// and uses ES modules.
        command_line->AppendSwitch("allow-file-access-from-files");
    }

private:
    IMPLEMENT_REFCOUNTING(NebulaApp);
};

int RunNebula(HINSTANCE instance) {
    CefMainArgs main_args(instance);
    CefRefPtr<NebulaApp> app(new NebulaApp);

    const int subprocess_exit_code =
        CefExecuteProcess(main_args, app, nullptr);
    if (subprocess_exit_code >= 0) {
        return subprocess_exit_code;
    }

    CefSettings settings;
    settings.no_sandbox = true;

    if (!CefInitialize(main_args, settings, app, nullptr)) {
        return CefGetExitCode();
    }

    CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
    command_line->InitFromString(GetCommandLineW());

    std::string url = command_line->GetSwitchValue("url");
    if (url.empty()) {
        url = GetHomeUrl();
    }

    CefWindowInfo window_info;
    window_info.SetAsPopup(nullptr, "Nebula Browser");

    CefBrowserSettings browser_settings;
    CefRefPtr<NebulaClient> client(new NebulaClient);

    if (!CefBrowserHost::CreateBrowser(
            window_info, client, url, browser_settings, nullptr, nullptr)) {
        CefShutdown();
        return 1;
    }

    CefRunMessageLoop();
    CefShutdown();

    return 0;
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance,
                      HINSTANCE previous_instance,
                      LPWSTR command_line,
                      int show_command) {
    UNREFERENCED_PARAMETER(previous_instance);
    UNREFERENCED_PARAMETER(command_line);
    UNREFERENCED_PARAMETER(show_command);

    return RunNebula(instance);
}
