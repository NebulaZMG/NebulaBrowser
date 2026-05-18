#include "platform/startup.h"

#include <fcntl.h>
#include <filesystem>
#include <system_error>
#include <sys/file.h>
#include <unistd.h>

#include "include/cef_command_line.h"
#include "ui/paths.h"

namespace nebula::platform {
namespace {

int g_single_instance_lock = -1;

}  // namespace

void PrepareApp() {}

bool TryAcquireSingleInstance() {
    const auto lock_path = nebula::ui::GetUserDataDirectory() / ".nebula_instance.lock";
    std::error_code ec;
    std::filesystem::create_directories(lock_path.parent_path(), ec);

    g_single_instance_lock = open(lock_path.c_str(), O_CREAT | O_RDWR, 0644);
    if (g_single_instance_lock < 0) {
        return true;
    }

    return flock(g_single_instance_lock, LOCK_EX | LOCK_NB) == 0;
}

CefMainArgs MakeMainArgs(const AppStartup& startup) {
    return CefMainArgs(startup.argc, startup.argv);
}

void InitCommandLine(CefRefPtr<CefCommandLine> command_line, const AppStartup& startup) {
    command_line->InitFromArgv(startup.argc, startup.argv);
}

void ConfigureCefSettings(CefSettings& settings) {
    const std::string user_data_dir = nebula::ui::GetUserDataDirectory().string();
    const std::string cache_dir = nebula::ui::GetCacheDirectory().string();
    if (!user_data_dir.empty()) {
        CefString(&settings.root_cache_path).FromString(user_data_dir);
    }
    if (!cache_dir.empty()) {
        CefString(&settings.cache_path).FromString(cache_dir);
    }
}

}  // namespace nebula::platform
