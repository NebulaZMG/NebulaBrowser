#include "cef/nebula_app.h"

#include "include/cef_process_message.h"
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

        if (name != "postMessage") {
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
}

void NebulaApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Context> context) {
    CEF_REQUIRE_RENDERER_THREAD();
    UNREFERENCED_PARAMETER(browser);
    UNREFERENCED_PARAMETER(frame);

    CefRefPtr<CefV8Value> global = context->GetGlobal();
    CefRefPtr<CefV8Value> native = CefV8Value::CreateObject(nullptr, nullptr);
    native->SetValue(
        "postMessage",
        CefV8Value::CreateFunction("postMessage", new NativeBridgeHandler()),
        V8_PROPERTY_ATTRIBUTE_NONE);
    global->SetValue("nebulaNative", native, V8_PROPERTY_ATTRIBUTE_READONLY);
}

}  // namespace nebula::cef
