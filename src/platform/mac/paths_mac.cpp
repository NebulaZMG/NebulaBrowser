#include "platform/paths_platform.h"

#include <mach-o/dyld.h>
#include <pwd.h>
#include <unistd.h>

#include <cstdlib>

namespace nebula::platform {

std::filesystem::path ExecutableDirectory() {
    char buffer[4096] = {};
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0) {
        return {};
    }

    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path DefaultUserDataRoot() {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Library" / "Application Support";
    }

    if (passwd* pw = getpwuid(getuid())) {
        return std::filesystem::path(pw->pw_dir) / "Library" / "Application Support";
    }

    return ExecutableDirectory();
}

std::string PathToUtf8(const std::filesystem::path& path) {
    return path.string();
}

}  // namespace nebula::platform
