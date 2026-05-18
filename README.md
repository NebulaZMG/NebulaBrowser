# Nebula Browser

A Chromium Embedded Framework (CEF) browser with a custom HTML chrome UI.

## Documentation

- **[Cross-platform build & architecture](docs/cross-platform.md)** — how to use one repo for Windows, macOS, and Linux; CEF setup; source layout; porting status.

## Quick start (Windows)

1. Download the [CEF standard binary distribution](https://cef-builds.spotifycdn.com/index.html) for Windows 64-bit.
2. Extract into `thirdparty/cef/` so `thirdparty/cef/cmake/FindCEF.cmake` exists.
3. Build:

   ```powershell
   cmake -B build
   cmake --build build --config Release
   ```

4. Run `build\Release\NebulaBrowser.exe`.

For macOS/Linux prerequisites, directory structure, and current port status, see [docs/cross-platform.md](docs/cross-platform.md).

## License

See the CEF distribution and Chromium license terms for third-party runtime components.
