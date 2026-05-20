#include "platform/startup.h"

#include <windows.h>

#include <string>

#include "include/cef_command_line.h"
#include "ui/paths.h"

namespace nebula::platform {
namespace {

constexpr wchar_t kMainInstanceMutexName[] = L"Local\\NebulaBrowserMainInstance";
constexpr wchar_t kWindowClassName[] = L"NebulaBrowserWindow";
constexpr ULONG_PTR kOpenTargetCopyDataId = 0x4E42554CUL;

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

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }

    std::wstring result(size, L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

void ForwardLaunchTargetToExistingWindow(const std::string& launch_target) {
    if (launch_target.empty()) {
        return;
    }

    HWND existing_window = FindWindowW(kWindowClassName, nullptr);
    if (!existing_window) {
        return;
    }

    const std::wstring target = Utf8ToWide(launch_target);
    if (target.empty()) {
        return;
    }

    COPYDATASTRUCT data = {};
    data.dwData = kOpenTargetCopyDataId;
    data.cbData = static_cast<DWORD>((target.size() + 1) * sizeof(wchar_t));
    data.lpData = const_cast<wchar_t*>(target.c_str());
    SendMessageW(existing_window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data));

    ShowWindow(existing_window, SW_SHOWNORMAL);
    SetForegroundWindow(existing_window);
}

}  // namespace

void PrepareApp() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

bool TryAcquireSingleInstance(const std::string& launch_target) {
    static ScopedHandle mutex(CreateMutexW(nullptr, TRUE, kMainInstanceMutexName));
    const bool already_running = mutex.valid() && GetLastError() == ERROR_ALREADY_EXISTS;
    if (already_running) {
        ForwardLaunchTargetToExistingWindow(launch_target);
    }
    return !already_running;
}

CefMainArgs MakeMainArgs(const AppStartup& startup) {
    return CefMainArgs(static_cast<HINSTANCE>(startup.instance));
}

void InitCommandLine(CefRefPtr<CefCommandLine> command_line, const AppStartup& startup) {
    NEBULA_UNUSED(startup);
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
