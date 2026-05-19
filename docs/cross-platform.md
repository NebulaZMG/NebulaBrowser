# Cross-platform build guide

Nebula Browser uses a **single codebase** and **one git branch** for Windows, macOS, and Linux. You do not maintain separate platform branches. Instead, each machine unpacks the correct [CEF](https://bitbucket.org/chromiumembedded/cef) binary distribution into `thirdparty/cef/` and builds with CMake.

## Overview

| Layer | Location | Platform-specific? |
|-------|----------|-------------------|
| CEF binaries | `thirdparty/cef/` (gitignored) | Yes — one distribution per OS |
| CMake / deploy | `CMakeLists.txt` | Yes — uses CEF’s `COPY_FILES`, `COPY_MAC_FRAMEWORK`, etc. |
| App entry & CEF init | `src/app/run.cpp`, `src/platform/*/startup_*.cpp` | Partly |
| Native window shell | `src/platform/win\|mac\|linux/` | Yes |
| Browser embedding | `src/platform/*/browser_host_*.cpp` | Partly |
| Tabs, URLs, UI routing | `src/browser/`, `src/ui/`, `src/cef/` | No (shared) |

**Windows** has a full native shell (custom frame, DWM, embedded CEF views).

**macOS** has a Cocoa native shell (`NSWindow` / `NSView`) with CEF child-view embedding.

**Linux** compiles and links today, but `NebulaWindow::Create()` is still a stub that returns `false` until a real X11 (or GTK) host is implemented.

## Directory layout

```
thirdparty/cef/          # CEF standard distribution for your OS (not in git)

src/platform/
  types.h                # Rect, BrowserLayout, NativeWindow, AppStartup
  startup.h              # PrepareApp, single-instance, CefMainArgs, CefSettings paths
  browser_host.h         # CefWindowInfo helpers, resize/show/destroy browser HWNDs
  paths_platform.h       # ExecutableDirectory, DefaultUserDataRoot, PathToUtf8

  win/                   # Windows implementation
  mac/                   # macOS Cocoa implementation
  linux/                 # Linux stubs + partial CEF glue

src/window/
  nebula_window.h        # Platform-agnostic window API (PIMPL)

src/app/                 # Shared controller and run loop
src/browser/             # Shared tab/session logic
src/cef/                 # Shared CEF app and browser client
src/ui/                  # Shared nebula:// URLs and path helpers
ui/                      # HTML/JS UI (copied next to the binary at build time)
```

## Prerequisites

- **CMake** 3.21+
- **C++20** compiler  
  - Windows: Visual Studio 2022  
  - macOS: Xcode 16+ (Clang)  
  - Linux: GCC 10+ or Clang, plus development packages for X11 (CEF’s CMake runs `pkg-config` for X11 on Linux)
- **CEF standard binary distribution** for your OS and CPU architecture from the [CEF builds](https://cef-builds.spotifycdn.com/index.html) page (or your usual CEF source)

## CEF setup

1. Download the **standard distribution** for your platform (e.g. `cef_binary_*_windows64`, `*_macos*`, `*_linux64`).
2. Extract so that this path exists:

   ```
   thirdparty/cef/cmake/FindCEF.cmake
   ```

3. The rest of the distribution (`Release/`, `Resources/`, `libcef_dll/`, etc.) should sit directly under `thirdparty/cef/` as shipped by CEF.

`thirdparty/cef/` is listed in `.gitignore` so binaries are never committed. Each developer and CI machine supplies its own copy.

## IDE / clangd setup

Without CEF headers, clangd will show many errors (unknown types like `CefWindowInfo`, `CefMainArgs`, etc.). To get full IDE support:

1. Unpack CEF into `thirdparty/cef/` as described above
2. Generate `compile_commands.json`:
   ```bash
   cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
   ```
3. Symlink it to the project root:
   ```bash
   ln -s build/compile_commands.json .
   ```

The included `.clangd` file provides fallback flags, but full diagnostics require the compile database.

Optional: override the location at configure time:

```bash
cmake -B build -DCEF_ROOT=/path/to/cef_binary
```

## Building

### Windows

```powershell
cmake -B build
cmake --build build --config Release
```

Output: `build/Release/NebulaBrowser.exe` (plus CEF DLLs and `ui/` copied beside it).

### macOS

```bash
cmake -B build -DPROJECT_ARCH=x86_64   # or arm64 on Apple Silicon
cmake --build build --config Release
```

CMake builds a `NebulaBrowser.app` bundle and copies the Chromium Embedded Framework into `Contents/Frameworks`. The macOS port includes a Cocoa `NSWindow` / `NSView` host and child-view embedding for CEF browsers.

CEF **helper app** subprocess targets may still need to be added if the selected CEF distribution does not provide a compatible default subprocess layout. Use CEF’s `cefsimple` sample as the reference for the complete Mac bundle layout.

### Linux

```bash
cmake -B build
cmake --build build
```

Output: `build/NebulaBrowser` (or `build/<Config>/NebulaBrowser` depending on generator). CEF `.so` files and resources are copied next to the executable.

You may need to set **SUID** on `chrome-sandbox` as printed by CMake post-build (CEF documents this in its Linux README).

Until a Linux host window exists, the binary will not show a UI for the same reason as macOS.

## How CMake picks a platform

`find_package(CEF)` loads CEF’s `cef_variables.cmake`, which sets:

- `OS_WINDOWS`
- `OS_MACOSX`
- `OS_LINUX`

`CMakeLists.txt` then:

1. Adds the matching `src/platform/{win,mac,linux}/*.cpp` sources
2. Sets the executable type (`WIN32`, `MACOSX_BUNDLE`, or plain executable)
3. Links `libcef_dll_wrapper`, `libcef_lib`, and `${CEF_STANDARD_LIBS}`
4. Runs CEF’s copy macros so runtime files land beside the app

Shared sources are always compiled; only the `src/platform/...` tree changes per OS.

## Application flow

```
main (app/main.cpp)
  └─ RunNebula(AppStartup)          src/app/run.cpp
       ├─ platform::PrepareApp()
       ├─ CefExecuteProcess()       (subprocesses)
       ├─ platform::TryAcquireSingleInstance()
       ├─ CefInitialize()
       ├─ NebulaController::Create()
       │    └─ NebulaWindow::Create()   platform-specific
       └─ CefRunMessageLoop()
```

`AppStartup` carries platform entry data:

- **Windows:** `HINSTANCE` + show command (`wWinMain`)
- **macOS / Linux:** `argc` / `argv` (`main`)

## User data and profile paths

Shared logic in `src/ui/paths.cpp` writes under:

| OS | Default root |
|----|----------------|
| Windows | `%LOCALAPPDATA%\Nebula\User Data` (falls back to exe directory) |
| macOS | `~/Library/Application Support/Nebula/User Data` |
| Linux | `$XDG_DATA_HOME/Nebula/User Data` or `~/.local/share/Nebula/User Data` |

Platform-specific discovery lives in `src/platform/*/paths_*.cpp`.

## GPU / Chromium flags

`src/cef/nebula_app.cpp` applies shared switches (`no-sandbox`, `ignore-gpu-blocklist`, etc.) and platform graphics backends:

| OS | Backend |
|----|---------|
| Windows | ANGLE + D3D11 |
| macOS | ANGLE + Metal |
| Linux | EGL |

## Adding or changing platform code

1. **Shared behavior** (tabs, `nebula://` URLs, CEF clients) → edit under `src/browser/`, `src/ui/`, `src/cef/`, `src/app/` only if it is truly cross-platform.
2. **Native window or OS API** → edit `src/platform/<os>/` and keep `src/window/nebula_window.h` free of `#include <windows.h>` (or Cocoa/X11 headers).
3. **CEF child window embedding** → `src/platform/<os>/browser_host_*.cpp` (`MakeChildWindowInfo`, resize, visibility).
4. **Startup / paths** → `startup_*.cpp` and `paths_*.cpp` in the same folder.
5. **New OS** → add `src/platform/<os>/`, extend the `if(OS_...)` blocks in `CMakeLists.txt`, and document CEF distribution layout here.

## Porting checklist (macOS / Linux)

- [x] macOS: implement `NebulaWindow` in `nebula_window_mac.mm`
- [x] macOS: wire `browser_host_mac.mm` resize/show/raise to Cocoa views
- [ ] Linux: implement `NebulaWindow` in `nebula_window_linux.cpp`
- [ ] Linux: wire `browser_host_linux.cpp` resize/show/raise to the real toolkit
- [ ] macOS: add CEF helper app targets and bundle layout (copy from `cefsimple`)
- [ ] Linux: confirm X11 (or chosen toolkit) parent handle for `CefWindowInfo::SetAsChild`
- [ ] Test GPU diagnostics page and hardware acceleration on target hardware
- [ ] Package or script runtime layout (`ui/`, CEF frameworks/libs, ICU locales, etc.)

## FAQ

**Do I need a separate git branch per OS?**  
No. Use one branch; swap `thirdparty/cef` and rebuild on each machine.

**Can I commit CEF into the repo?**  
Not recommended (size, licensing, per-arch binaries). Keeping `thirdparty/cef/` gitignored is intentional.

**Why does Linux build but not run?**
The Linux window stub intentionally returns `false` from `Create()` until a native shell exists. Shared CEF and browser code are ready; the missing piece is the host window.

**Where is the Windows UI code?**  
`src/platform/win/nebula_window_win.cpp` — custom chrome, hit-testing, fullscreen, and child browser placement.
