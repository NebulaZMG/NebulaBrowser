#include "cef/nebula_app.h"

#include "include/cef_process_message.h"
#include "platform/types.h"
#include "include/wrapper/cef_helpers.h"

namespace nebula::cef {
namespace {

constexpr char kChromeCommandMessage[] = "NebulaChromeCommand";

class NativeBridgeHandler final : public CefV8Handler {
public:
    bool Execute(const CefString& name,
                 CefRefPtr<CefV8Value> object,
                 const CefV8ValueList& arguments,
                 CefRefPtr<CefV8Value>& retval,
                 CefString& exception) override {
        UNREFERENCED_PARAMETER(object);
        UNREFERENCED_PARAMETER(retval);

        if (name != "postMessage" && name != "sendToHost" && name != "send") {
            return false;
        }

        if (arguments.empty() || !arguments[0]->IsString()) {
            exception = "nebulaNative.postMessage requires a command string.";
            return true;
        }

        CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
        CefRefPtr<CefBrowser> browser = context ? context->GetBrowser() : nullptr;
        CefRefPtr<CefFrame> frame = context ? context->GetFrame() : nullptr;
        if (!browser || !frame) {
            exception = "No CEF frame is available for native messaging.";
            return true;
        }

        CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create(kChromeCommandMessage);
        CefRefPtr<CefListValue> args = message->GetArgumentList();
        args->SetString(0, arguments[0]->GetStringValue());
        args->SetString(1, arguments.size() > 1 && arguments[1]->IsString()
                               ? arguments[1]->GetStringValue()
                               : CefString());

        frame->SendProcessMessage(PID_BROWSER, message);
        return true;
    }

private:
    IMPLEMENT_REFCOUNTING(NativeBridgeHandler);
};

}  // namespace

void NebulaApp::OnBeforeCommandLineProcessing(const CefString& process_type,
                                              CefRefPtr<CefCommandLine> command_line) {
    UNREFERENCED_PARAMETER(process_type);

    // The bundled UI is loaded from file:// and uses ES modules.
    command_line->AppendSwitch("allow-file-access-from-files");

    // CefSettings.no_sandbox disables the browser-level sandbox, but Chromium
    // still attempts to bring up a separate GPU sandbox inside the GPU process.
    // Without the host-side sandbox plumbing this fails with STATUS_BREAKPOINT
    // (-2147483645) immediately on startup, which is exactly what the GPU
    // diagnostics page was showing - the GPU process crashed three times and
    // Chromium then fell back to software rendering. Disabling the GPU sandbox
    // matches the rest of our no_sandbox configuration and lets the GPU
    // process initialize.
    command_line->AppendSwitch("no-sandbox");
    command_line->AppendSwitch("disable-gpu-sandbox");
    command_line->AppendSwitch("in-process-gpu");

    // Avoid Chromium's conservative GPU blocklist, but let Chromium choose the
    // safest graphics backend for this machine. Forcing raster/zero-copy paths
    // can prevent WebGL shared contexts from initializing on some drivers.
    command_line->AppendSwitch("ignore-gpu-blocklist");
    command_line->AppendSwitch("enable-accelerated-video-decode");

#if defined(_WIN32)
    command_line->AppendSwitchWithValue("use-gl", "angle");
    command_line->AppendSwitchWithValue("use-angle", "d3d11");
#elif defined(__APPLE__)
    command_line->AppendSwitchWithValue("use-angle", "metal");
#else
    command_line->AppendSwitchWithValue("use-gl", "egl");
#endif
}

void NebulaApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Context> context) {
    CEF_REQUIRE_RENDERER_THREAD();
    UNREFERENCED_PARAMETER(browser);
    UNREFERENCED_PARAMETER(frame);

    CefRefPtr<CefV8Value> global = context->GetGlobal();
    CefRefPtr<NativeBridgeHandler> handler = new NativeBridgeHandler();
    CefRefPtr<CefV8Value> native = CefV8Value::CreateObject(nullptr, nullptr);
    native->SetValue(
        "postMessage",
        CefV8Value::CreateFunction("postMessage", handler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    global->SetValue("nebulaNative", native, V8_PROPERTY_ATTRIBUTE_READONLY);

    CefRefPtr<CefV8Value> electron_api = CefV8Value::CreateObject(nullptr, nullptr);
    electron_api->SetValue(
        "sendToHost",
        CefV8Value::CreateFunction("sendToHost", handler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    electron_api->SetValue(
        "send",
        CefV8Value::CreateFunction("send", handler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    global->SetValue("electronAPI", electron_api, V8_PROPERTY_ATTRIBUTE_READONLY);
}

}  // namespace nebula::cef
