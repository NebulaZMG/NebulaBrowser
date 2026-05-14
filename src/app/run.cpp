#include "app/run.h"

#include "app/nebula_controller.h"
#include "cef/nebula_app.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "ui/paths.h"

namespace nebula::app {
namespace {

constexpr wchar_t kMainInstanceMutexName[] = L"Local\\NebulaBrowserMainInstance";

void EnableDpiAwareness() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE handle) : handle_(handle) {}
    ~ScopedHandle() {
        if (handle_) {
            CloseHandle(handle_);
        }
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    bool valid() const { return handle_ != nullptr; }

private:
    HANDLE handle_ = nullptr;
};

}  // namespace

int RunNebula(HINSTANCE instance, int show_command) {
    EnableDpiAwareness();

    CefMainArgs main_args(instance);
    CefRefPtr<nebula::cef::NebulaApp> app(new nebula::cef::NebulaApp);

    const int subprocess_exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (subprocess_exit_code >= 0) {
        return subprocess_exit_code;
    }

    ScopedHandle main_instance_mutex(CreateMutexW(nullptr, TRUE, kMainInstanceMutexName));
    if (main_instance_mutex.valid() && GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    CefSettings settings;
    settings.no_sandbox = true;
    settings.persist_session_cookies = true;

    // A persistent profile is required for the GPU shader cache and several
    // hardware acceleration features. Without these Chromium silently falls
    // back to software rendering, which causes choppy video and disables
    // WebGL/WebGL2 in the GPU diagnostics page.
    const std::wstring user_data_dir = nebula::ui::GetUserDataDirectory().wstring();
    const std::wstring cache_dir = nebula::ui::GetCacheDirectory().wstring();
    if (!user_data_dir.empty()) {
        CefString(&settings.root_cache_path).FromWString(user_data_dir);
    }
    if (!cache_dir.empty()) {
        CefString(&settings.cache_path).FromWString(cache_dir);
    }

    if (!CefInitialize(main_args, settings, app, nullptr)) {
        return CefGetExitCode();
    }

    CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
    command_line->InitFromString(GetCommandLineW());

    std::string initial_url = command_line->GetSwitchValue("url");
    if (!initial_url.empty() && nebula::ui::IsChromiumNewTabUrl(initial_url)) {
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
