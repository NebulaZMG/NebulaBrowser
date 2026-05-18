#pragma once

#include "platform/types.h"

namespace nebula::app {

enum class AppMode {
    Desktop,
    BigPicture,
};

struct LaunchOptions {
    AppMode mode = AppMode::Desktop;
};

int RunNebula(const nebula::platform::AppStartup& startup, LaunchOptions options = {});

}  // namespace nebula::app
