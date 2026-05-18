#include "platform/startup.h"

#include <windows.h>

#include "include/cef_command_line.h"
#include "ui/paths.h"

namespace nebula::platform {
namespace {

constexpr wchar_t kMainInstanceMutexName[] = L"Local\\NebulaBrowserMainInstance";

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

void PrepareApp() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

bool TryAcquireSingleInstance() {
    static ScopedHandle mutex(CreateMutexW(nullptr, TRUE, kMainInstanceMutexName));
    return !(mutex.valid() && GetLastError() == ERROR_ALREADY_EXISTS);
}

CefMainArgs MakeMainArgs(const AppStartup& startup) {
    return CefMainArgs(static_cast<HINSTANCE>(startup.instance));
}

void InitCommandLine(CefRefPtr<CefCommandLine> command_line, const AppStartup& startup) {
    UNREFERENCED_PARAMETER(startup);
    command_line->InitFromString(::GetCommandLineW());
}

void ConfigureCefSettings(CefSettings& settings) {
    const std::wstring user_data_dir = nebula::ui::GetUserDataDirectory().wstring();
    const std::wstring cache_dir = nebula::ui::GetCacheDirectory().wstring();
    if (!user_data_dir.empty()) {
        CefString(&settings.root_cache_path).FromWString(user_data_dir);
    }
    if (!cache_dir.empty()) {
        CefString(&settings.cache_path).FromWString(cache_dir);
    }
}

}  // namespace nebula::platform
