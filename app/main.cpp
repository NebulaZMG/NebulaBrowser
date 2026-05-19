#include "app/run.h"
#include "platform/types.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include "include/wrapper/cef_library_loader.h"
#endif

#if defined(_WIN32)
int APIENTRY wWinMain(HINSTANCE instance,
                      HINSTANCE previous_instance,
                      LPWSTR command_line,
                      int show_command) {
    UNREFERENCED_PARAMETER(previous_instance);
    UNREFERENCED_PARAMETER(command_line);

    const nebula::platform::AppStartup startup{instance, show_command};
    return nebula::app::RunNebula(startup, {nebula::app::AppMode::Desktop});
}
#else
int main(int argc, char* argv[]) {
#if defined(__APPLE__)
    CefScopedLibraryLoader library_loader;
    if (!library_loader.LoadInMain()) {
        return 1;
    }
#endif

    const nebula::platform::AppStartup startup{argc, argv};
    return nebula::app::RunNebula(startup, {nebula::app::AppMode::Desktop});
}
#endif
