#pragma once

#include <filesystem>
#include <string>

namespace nebula::platform {

std::filesystem::path ExecutableDirectory();
std::filesystem::path DefaultUserDataRoot();
std::string PathToUtf8(const std::filesystem::path& path);

}  // namespace nebula::platform
