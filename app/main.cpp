#include <windows.h>

#include "app/run.h"

int APIENTRY wWinMain(HINSTANCE instance,
                      HINSTANCE previous_instance,
                      LPWSTR command_line,
                      int show_command) {
    UNREFERENCED_PARAMETER(previous_instance);
    UNREFERENCED_PARAMETER(command_line);

    return nebula::app::RunNebula(instance, show_command);
}
