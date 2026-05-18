#pragma once

namespace nebula::platform {

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct BrowserLayout {
    Rect chrome;
    Rect content;
};

using NativeWindow = void*;

#if defined(_WIN32)
struct AppStartup {
    void* instance = nullptr;
    int show_command = 0;
};
#else
struct AppStartup {
    int argc = 0;
    char** argv = nullptr;
};
#endif

}  // namespace nebula::platform
