#include "platform/paths_platform.h"

#include <pwd.h>
#include <unistd.h>

#include <cstdlib>

namespace nebula::platform {

std::filesystem::path ExecutableDirectory() {
    char buffer[4096] = {};
    const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0) {
        return {};
    }

    buffer[length] = '\0';
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path DefaultUserDataRoot() {
    if (const char* xdg_data = std::getenv("XDG_DATA_HOME"); xdg_data && *xdg_data) {
        return std::filesystem::path(xdg_data);
    }

    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local" / "share";
    }

    if (passwd* pw = getpwuid(getuid())) {
        return std::filesystem::path(pw->pw_dir) / ".local" / "share";
    }

    return ExecutableDirectory();
}

std::string PathToUtf8(const std::filesystem::path& path) {
    return path.string();
}

}  // namespace nebula::platform
