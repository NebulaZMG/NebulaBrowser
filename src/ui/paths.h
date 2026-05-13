#pragma once

#include <filesystem>
#include <string>

namespace nebula::ui {

std::filesystem::path GetExecutableDirectory();
std::filesystem::path GetUiPagePath(const std::wstring& page_name);
std::string FilePathToUrl(std::filesystem::path path);
std::string GetChromeUrl();
std::string GetHomeUrl();
std::string GetSettingsUrl();
std::string GetBigPictureUrl();
std::string GetMenuPopupUrl();

bool IsInternalHomeUrl(const std::string& url);
bool IsChromiumNewTabUrl(const std::string& url);
bool IsEmptyOrChromiumNewTabUrl(const std::string& url);

}  // namespace nebula::ui
