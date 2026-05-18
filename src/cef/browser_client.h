#pragma once

#include <string>
#include <vector>

#include "include/cef_client.h"
#include "include/cef_display_handler.h"
#include "include/cef_keyboard_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_permission_handler.h"
#include "include/cef_request_handler.h"

namespace nebula::cef {

enum class BrowserRole {
    Chrome,
    Content,
    BigPicture,
    MenuPopup,
};

class BrowserClientDelegate {
public:
    virtual ~BrowserClientDelegate() = default;
    virtual void OnBrowserCreated(BrowserRole role, CefRefPtr<CefBrowser> browser) = 0;
    virtual void OnBrowserClosing(BrowserRole role, CefRefPtr<CefBrowser> browser) = 0;
    virtual void OnChromeCommand(const std::string& command, const std::string& payload) = 0;
    virtual void OnContentAddressChanged(CefRefPtr<CefBrowser> browser, const std::string& url) = 0;
    virtual void OnContentTitleChanged(CefRefPtr<CefBrowser> browser, const std::string& title) = 0;
    virtual void OnContentLoadingStateChanged(CefRefPtr<CefBrowser> browser, bool is_loading) = 0;
    virtual void OnContentLoadProgressChanged(CefRefPtr<CefBrowser> browser, double progress) = 0;
    virtual void OnContentLoadFinished(CefRefPtr<CefBrowser> browser, const std::string& url) = 0;
    virtual void OnContentFaviconChanged(CefRefPtr<CefBrowser> browser, const std::vector<std::string>& urls) = 0;
    virtual void OnContentFullscreenChanged(CefRefPtr<CefBrowser> browser, bool fullscreen) = 0;
    virtual void OnPopupRequested(CefRefPtr<CefBrowser> browser, const std::string& target_url) = 0;
    virtual bool ShouldBypassInsecureWarning(CefRefPtr<CefBrowser> browser, const std::string& target_url) = 0;
};

class NebulaBrowserClient final : public CefClient,
                                 public CefDisplayHandler,
                                 public CefKeyboardHandler,
                                 public CefLifeSpanHandler,
                                 public CefLoadHandler,
                                 public CefPermissionHandler,
                                 public CefRequestHandler {
public:
    NebulaBrowserClient(BrowserRole role, BrowserClientDelegate* delegate);

    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefPermissionHandler> GetPermissionHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefProcessId source_process,
                                  CefRefPtr<CefProcessMessage> message) override;

    void OnAddressChange(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         const CefString& url) override;
    void OnTitleChange(CefRefPtr<CefBrowser> browser,
                       const CefString& title) override;
    void OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
                            const std::vector<CefString>& icon_urls) override;
    void OnFullscreenModeChange(CefRefPtr<CefBrowser> browser, bool fullscreen) override;

    bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                       const CefKeyEvent& event,
                       CefEventHandle os_event,
                       bool* is_keyboard_shortcut) override;

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
                       bool* no_javascript_access) override;

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                              bool isLoading,
                              bool canGoBack,
                              bool canGoForward) override;
    void OnLoadStart(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     TransitionType transition_type) override;
    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override;

    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request,
                        bool user_gesture,
                        bool is_redirect) override;

    bool OnShowPermissionPrompt(CefRefPtr<CefBrowser> browser,
                                uint64_t prompt_id,
                                const CefString& requesting_origin,
                                uint32_t requested_permissions,
                                CefRefPtr<CefPermissionPromptCallback> callback) override;

private:
    BrowserRole role_;
    BrowserClientDelegate* delegate_ = nullptr;

    IMPLEMENT_REFCOUNTING(NebulaBrowserClient);
};

}  // namespace nebula::cef
