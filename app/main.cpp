#include "app/run.h"
#include "platform/types.h"

#if defined(_WIN32)
#include <windows.h>

int APIENTRY wWinMain(HINSTANCE instance,
                      HINSTANCE previous_instance,
                      LPWSTR command_line,
                      int show_command) {
    UNREFERENCED_PARAMETER(previous_instance);
    UNREFERENCED_PARAMETER(command_line);

    const nebula::platform::AppStartup startup{instance, show_command};
    return nebula::app::RunNebula(startup);
}
#else
int main(int argc, char* argv[]) {
    const nebula::platform::AppStartup startup{argc, argv};
    return nebula::app::RunNebula(startup);
}
#endif
