#include "cef/nebula_app.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_library_loader.h"

#if defined(CEF_USE_SANDBOX)
#include "include/cef_sandbox_mac.h"
#endif

int main(int argc, char* argv[]) {
#if defined(CEF_USE_SANDBOX)
    CefScopedSandboxContext sandbox_context;
    if (!sandbox_context.Initialize(argc, argv)) {
        return 1;
    }
#endif

    CefScopedLibraryLoader library_loader;
    if (!library_loader.LoadInHelper()) {
        return 1;
    }

    const CefMainArgs main_args(argc, argv);
    CefRefPtr<nebula::cef::NebulaApp> app(new nebula::cef::NebulaApp);
    return CefExecuteProcess(main_args, app, nullptr);
}
