#include "app/run.h"

#include "app/nebula_controller.h"
#include "cef/nebula_app.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "platform/startup.h"
#include "ui/paths.h"

namespace nebula::app {

int RunNebula(const nebula::platform::AppStartup& startup, LaunchOptions options) {
    nebula::platform::PrepareApp();

    const CefMainArgs main_args = nebula::platform::MakeMainArgs(startup);
    CefRefPtr<nebula::cef::NebulaApp> app(new nebula::cef::NebulaApp);

    const int subprocess_exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (subprocess_exit_code >= 0) {
        return subprocess_exit_code;
    }

    if (!nebula::platform::TryAcquireSingleInstance()) {
        return 0;
    }

    CefSettings settings;
    settings.no_sandbox = true;
    settings.persist_session_cookies = true;
    nebula::platform::ConfigureCefSettings(settings);

    if (!CefInitialize(main_args, settings, app, nullptr)) {
        return CefGetExitCode();
    }

    CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
    nebula::platform::InitCommandLine(command_line, startup);

    std::string initial_url = command_line->GetSwitchValue("url");
    if (!initial_url.empty() && nebula::ui::IsChromiumNewTabUrl(initial_url)) {
        initial_url = nebula::ui::GetHomeUrl();
    }

    NebulaController controller(startup, std::move(initial_url), options);
    const bool created = controller.Create();
    if (created) {
        CefRunMessageLoop();
    }

    CefShutdown();
    return created ? 0 : 1;
}

}  // namespace nebula::app
