#pragma once

#include "include/cef_app.h"
#include "platform/types.h"

namespace nebula::platform {

void PrepareApp();
bool TryAcquireSingleInstance();
CefMainArgs MakeMainArgs(const AppStartup& startup);
void InitCommandLine(CefRefPtr<CefCommandLine> command_line, const AppStartup& startup);
void ConfigureCefSettings(CefSettings& settings);

}  // namespace nebula::platform
