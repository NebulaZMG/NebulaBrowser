#include "app/run.h"

#include "app/nebula_controller.h"
#include "browser/url_utils.h"
#include "cef/nebula_app.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "platform/default_browser.h"
#include "platform/startup.h"
#include "ui/paths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace nebula::app {
namespace {

std::string Trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool StartsWithKnownScheme(const std::string& value) {
    const std::string lower = ToLowerAscii(value);
    return lower.starts_with("http://") ||
           lower.starts_with("https://") ||
           lower.starts_with("file:") ||
           lower.starts_with("data:") ||
           lower.starts_with("blob:") ||
           lower.starts_with("chrome:") ||
           lower.starts_with("nebula://");
}

std::filesystem::path PathFromUtf8(const std::string& value) {
#if defined(_WIN32)
    const int size = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return std::filesystem::path(value);
    }

    std::wstring wide(size, L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), size);
    return std::filesystem::path(wide);
#else
    return std::filesystem::path(value);
#endif
}

std::string NormalizeLaunchTarget(std::string target) {
    target = Trim(std::move(target));
    if (target.empty() || nebula::ui::IsChromiumNewTabUrl(target)) {
        return target.empty() ? std::string{} : nebula::ui::GetHomeUrl();
    }

    if (StartsWithKnownScheme(target)) {
        return target;
    }

    const std::filesystem::path path = PathFromUtf8(target);
    std::error_code ec;
    if (!path.empty() && std::filesystem::exists(path, ec) && !ec) {
        const std::filesystem::path absolute_path = std::filesystem::absolute(path, ec);
        return nebula::ui::FilePathToUrl(ec ? path : absolute_path);
    }

    return nebula::browser::NormalizeNavigationInput(target);
}

std::string GetLaunchTarget(CefRefPtr<CefCommandLine> command_line) {
    if (!command_line) {
        return {};
    }

    std::string target = command_line->GetSwitchValue("url");
    if (!target.empty()) {
        return NormalizeLaunchTarget(std::move(target));
    }

    std::vector<CefString> arguments;
    command_line->GetArguments(arguments);
    for (const auto& argument : arguments) {
        target = argument.ToString();
        if (!Trim(target).empty()) {
            return NormalizeLaunchTarget(std::move(target));
        }
    }

    return {};
}

}  // namespace

int RunNebula(const nebula::platform::AppStartup& startup, LaunchOptions options) {
    nebula::platform::PrepareApp();

    const CefMainArgs main_args = nebula::platform::MakeMainArgs(startup);
    CefRefPtr<nebula::cef::NebulaApp> app(new nebula::cef::NebulaApp);

    const int subprocess_exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (subprocess_exit_code >= 0) {
        return subprocess_exit_code;
    }

    CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
    nebula::platform::InitCommandLine(command_line, startup);
    std::string initial_url = GetLaunchTarget(command_line);

    if (!nebula::platform::TryAcquireSingleInstance(initial_url)) {
        return 0;
    }
    nebula::platform::EnsureDefaultBrowserRegistration();

    CefSettings settings;
    settings.no_sandbox = true;
    settings.persist_session_cookies = true;
    nebula::platform::ConfigureCefSettings(settings);

    if (!CefInitialize(main_args, settings, app, nullptr)) {
        return CefGetExitCode();
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
