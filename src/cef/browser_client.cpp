#include "cef/browser_client.h"

#include "include/cef_request.h"
#include "include/wrapper/cef_helpers.h"
#include "ui/paths.h"

namespace nebula::cef {
namespace {

constexpr char kChromeCommandMessage[] = "NebulaChromeCommand";

std::vector<std::string> ToStringVector(const std::vector<CefString>& values) {
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const auto& value : values) {
        result.push_back(value.ToString());
    }
    return result;
}

}  // namespace

NebulaBrowserClient::NebulaBrowserClient(BrowserRole role, BrowserClientDelegate* delegate)
    : role_(role), delegate_(delegate) {}

bool NebulaBrowserClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                                   CefRefPtr<CefFrame> frame,
                                                   CefProcessId source_process,
                                                   CefRefPtr<CefProcessMessage> message) {
    CEF_REQUIRE_UI_THREAD();
    UNREFERENCED_PARAMETER(browser);
    UNREFERENCED_PARAMETER(frame);
    UNREFERENCED_PARAMETER(source_process);

    if ((role_ != BrowserRole::Chrome && role_ != BrowserRole::MenuPopup) || !message ||
        message->GetName().ToString() != kChromeCommandMessage) {
        return false;
    }

    CefRefPtr<CefListValue> args = message->GetArgumentList();
    const std::string command = args && args->GetSize() > 0 ? args->GetString(0).ToString() : "";
    const std::string payload = args && args->GetSize() > 1 ? args->GetString(1).ToString() : "";
    if (delegate_ && !command.empty()) {
        delegate_->OnChromeCommand(command, payload);
        return true;
    }

    return false;
}

void NebulaBrowserClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefFrame> frame,
                                          const CefString& url) {
    CEF_REQUIRE_UI_THREAD();
    if (role_ == BrowserRole::Content && delegate_ && frame && frame->IsMain()) {
        delegate_->OnContentAddressChanged(browser, url.ToString());
    }
}

void NebulaBrowserClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                        const CefString& title) {
    CEF_REQUIRE_UI_THREAD();
    if (role_ == BrowserRole::Content && delegate_) {
        delegate_->OnContentTitleChanged(browser, title.ToString());
    }
}

void NebulaBrowserClient::OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
                                             const std::vector<CefString>& icon_urls) {
    CEF_REQUIRE_UI_THREAD();
    if (role_ == BrowserRole::Content && delegate_) {
        delegate_->OnContentFaviconChanged(browser, ToStringVector(icon_urls));
    }
}

bool NebulaBrowserClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                                        const CefKeyEvent& event,
                                        CefEventHandle os_event,
                                        bool* is_keyboard_shortcut) {
    CEF_REQUIRE_UI_THREAD();
    UNREFERENCED_PARAMETER(os_event);

    if (role_ == BrowserRole::Content &&
        event.type == KEYEVENT_RAWKEYDOWN &&
        (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0 &&
        event.windows_key_code == 'T') {
        if (is_keyboard_shortcut) {
            *is_keyboard_shortcut = true;
        }
        if (delegate_) {
            delegate_->OnPopupRequested(browser, nebula::ui::GetHomeUrl());
        }
        return true;
    }

    return false;
}

bool NebulaBrowserClient::OnBeforePopup(CefRefPtr<CefBrowser> browser,
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
                                        bool* no_javascript_access) {
    CEF_REQUIRE_UI_THREAD();
    UNREFERENCED_PARAMETER(frame);
    UNREFERENCED_PARAMETER(popup_id);
    UNREFERENCED_PARAMETER(target_frame_name);
    UNREFERENCED_PARAMETER(target_disposition);
    UNREFERENCED_PARAMETER(user_gesture);
    UNREFERENCED_PARAMETER(popupFeatures);
    UNREFERENCED_PARAMETER(windowInfo);
    UNREFERENCED_PARAMETER(client);
    UNREFERENCED_PARAMETER(settings);
    UNREFERENCED_PARAMETER(extra_info);
    UNREFERENCED_PARAMETER(no_javascript_access);

    if (role_ == BrowserRole::Content && delegate_) {
        delegate_->OnPopupRequested(browser, target_url.ToString());
        return true;
    }

    return false;
}

void NebulaBrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    if (delegate_) {
        delegate_->OnBrowserCreated(role_, browser);
    }
}

void NebulaBrowserClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    if (delegate_) {
        delegate_->OnBrowserClosing(role_, browser);
    }
}

void NebulaBrowserClient::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                               bool isLoading,
                                               bool canGoBack,
                                               bool canGoForward) {
    CEF_REQUIRE_UI_THREAD();
    UNREFERENCED_PARAMETER(canGoBack);
    UNREFERENCED_PARAMETER(canGoForward);

    if (role_ == BrowserRole::Content && delegate_) {
        delegate_->OnContentLoadingStateChanged(browser, isLoading);
    }
}

void NebulaBrowserClient::OnLoadStart(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      TransitionType transition_type) {
    CEF_REQUIRE_UI_THREAD();
    UNREFERENCED_PARAMETER(transition_type);

    if (role_ == BrowserRole::Content && delegate_ && frame && frame->IsMain()) {
        delegate_->OnContentLoadProgressChanged(browser, 0.12);
    }
}

void NebulaBrowserClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    int httpStatusCode) {
    CEF_REQUIRE_UI_THREAD();
    UNREFERENCED_PARAMETER(httpStatusCode);

    if (role_ == BrowserRole::Content && delegate_ && frame && frame->IsMain()) {
        delegate_->OnContentLoadProgressChanged(browser, 1.0);
    }
}

bool NebulaBrowserClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefRefPtr<CefRequest> request,
                                         bool user_gesture,
                                         bool is_redirect) {
    CEF_REQUIRE_UI_THREAD();
    UNREFERENCED_PARAMETER(browser);
    UNREFERENCED_PARAMETER(user_gesture);
    UNREFERENCED_PARAMETER(is_redirect);

    if (role_ == BrowserRole::Content && frame && frame->IsMain() && request &&
        nebula::ui::IsChromiumNewTabUrl(request->GetURL().ToString())) {
        frame->LoadURL(nebula::ui::GetHomeUrl());
        return true;
    }

    return false;
}

bool NebulaBrowserClient::OnShowPermissionPrompt(
    CefRefPtr<CefBrowser> browser,
    uint64_t prompt_id,
    const CefString& requesting_origin,
    uint32_t requested_permissions,
    CefRefPtr<CefPermissionPromptCallback> callback) {
    CEF_REQUIRE_UI_THREAD();
    UNREFERENCED_PARAMETER(prompt_id);
    UNREFERENCED_PARAMETER(requesting_origin);

    if (role_ == BrowserRole::Content &&
        (requested_permissions & CEF_PERMISSION_TYPE_GEOLOCATION) != 0 &&
        browser && callback &&
        nebula::ui::IsInternalHomeUrl(browser->GetMainFrame()->GetURL().ToString())) {
        callback->Continue(CEF_PERMISSION_RESULT_ACCEPT);
        return true;
    }

    return false;
}

}  // namespace nebula::cef
