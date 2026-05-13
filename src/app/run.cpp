#include "app/run.h"

#include "app/nebula_controller.h"
#include "cef/nebula_app.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "ui/paths.h"

namespace nebula::app {
namespace {

void EnableDpiAwareness() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

}  // namespace

int RunNebula(HINSTANCE instance, int show_command) {
    EnableDpiAwareness();

    CefMainArgs main_args(instance);
    CefRefPtr<nebula::cef::NebulaApp> app(new nebula::cef::NebulaApp);

    const int subprocess_exit_code = CefExecuteProcess(main_args, app, nullptr);
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

    std::string initial_url = command_line->GetSwitchValue("url");
    if (nebula::ui::IsEmptyOrChromiumNewTabUrl(initial_url)) {
        initial_url = nebula::ui::GetHomeUrl();
    }

    NebulaController controller(instance, initial_url, show_command);
    const bool created = controller.Create();
    if (created) {
        CefRunMessageLoop();
    }

    CefShutdown();
    return created ? 0 : 1;
}

}  // namespace nebula::app
