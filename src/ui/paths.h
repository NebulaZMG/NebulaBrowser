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
std::string GetDownloadsUrl();
std::string GetBigPictureUrl();
std::string GetGpuDiagnosticsUrl();
std::string GetMenuPopupUrl();
std::string GetInsecureWarningUrl(const std::string& target_url);
std::string GetNotFoundUrl(const std::string& target_url);
std::string ResolveInternalUrl(const std::string& url);
std::string ToInternalUrl(const std::string& url);

bool IsInternalHomeUrl(const std::string& url);
bool IsNebulaInternalUrl(const std::string& url);
bool IsHttpUrl(const std::string& url);
bool IsChromiumNewTabUrl(const std::string& url);
bool IsEmptyOrChromiumNewTabUrl(const std::string& url);

}  // namespace nebula::ui
