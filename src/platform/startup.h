#pragma once

#include "include/cef_app.h"
#include "platform/types.h"

#include <string>

namespace nebula::platform {

void PrepareApp();
bool TryAcquireSingleInstance(const std::string& launch_target = {});
CefMainArgs MakeMainArgs(const AppStartup& startup);
void InitCommandLine(CefRefPtr<CefCommandLine> command_line, const AppStartup& startup);
void ConfigureCefSettings(CefSettings& settings);

}  // namespace nebula::platform
