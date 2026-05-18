# Nebula Browser

A Chromium Embedded Framework (CEF) browser with a custom HTML chrome UI.

**Nebula Browser** is a controller-first web browser designed for Steam Deck, SteamOS-style systems, handheld PCs, living room setups, and gamepad-driven desktop environments.

Originally developed as an Electron-based browser for SteamOS users, Nebula Browser is now being rebuilt with **CEF / Chromium Embedded Framework** to provide deeper control over the browser experience, improved native integration, and a cleaner long-term foundation for custom UI, controller navigation, and platform-specific builds.

Development on Nebula Browser has somewhat resumed, with the project now moving toward a more native custom browser architecture.

Nebula Browser is intended to remain open source. The browser source code is licensed under MPL-2.0, while Nebula branding and visual assets are protected separately.

## Documentation

- **[Cross-platform build & architecture](docs/cross-platform.md)** — how to use one repo for Windows, macOS, and Linux; CEF setup; source layout; porting status.

---

## Project Status

Nebula Browser is currently in early active redevelopment.

The original Electron version proved the core idea: a browser built around controller use, large-screen navigation, and a UI that does not feel like a standard desktop browser awkwardly placed on a handheld gaming device.

The new CEF version is the next major step.

Current focus areas include:

* Rebuilding the browser around CEF
* Creating a fully custom browser interface
* Moving away from default Chromium UI elements
* Reusing and adapting existing Nebula UI concepts where possible
* Supporting controller-first navigation
* Preparing for cross-platform builds across Windows, Linux, and macOS
* Creating a stronger foundation for future NebulaOS integration

---

## Why CEF?

Nebula Browser originally used Electron because it was fast to prototype, easy to package, and allowed the project to move quickly. However, Electron also comes with limits when building a browser that is meant to feel fully custom and deeply integrated with the operating system.

The move to **CEF** allows Nebula Browser to become more than a web app wrapped in a browser shell.

CEF gives the project:

* More control over the native window and browser lifecycle
* Better separation between the browser engine and the custom UI
* Greater flexibility for building a non-Chrome-like interface
* A more suitable foundation for SteamOS, Linux, and future NebulaOS use
* The ability to build a browser that feels purpose-built rather than wrapped

The goal is not to create another Chromium clone. The goal is to create a browser interface designed around how people actually use handhelds, controllers, TVs, and gaming-first systems.

---

## Distribution Direction

Nebula Browser was originally aimed at Steam. However, after exploring Steam as a release platform, it became clear that Steam may not be the best fit for this kind of application.

The current distribution direction is:

### Primary Target

* **Itch.io**

Itch.io is currently the most practical and flexible platform for distributing Nebula Browser. It allows the project to be released as a utility-style app without needing to fit into a traditional game store category.

### Potential Future Targets

* **Epic Games Store**
* **GOG**
* **Flatpak / Linux package distribution**
* **Direct downloads from the Nebula website**

Epic Games and GOG are being considered as possible future distribution platforms, especially if Nebula Browser grows into a more polished desktop or gaming utility. These are not confirmed release platforms yet, but they remain possible options.

---

## Open Source Direction

Nebula Browser is intended to remain open source because browsers are privacy-sensitive software. Users should be able to inspect how the browser works, verify that it is not doing anything suspicious, and contribute fixes or improvements when they find problems.

Open source is especially important for:

* User trust
* Privacy transparency
* Linux and Steam Deck community adoption
* Cross-platform testing
* Community contributions
* Long-term sustainability

The core Nebula Browser project should remain open source, including:

* Browser shell
* CEF integration code
* Custom browser UI
* Controller navigation systems
* Settings and theme systems
* Build scripts and documentation

Not every Nebula-related component has to be open source. Some future or external components may remain private or be licensed separately, such as:

* NebulaOS-specific services
* Account systems
* Cloud sync
* Hosted backend services
* Internal analytics or crash reporting infrastructure
* Private distribution tooling
* Experimental unreleased features
* Protected branding assets

Suggested openness model:

```text
Nebula Browser: open source
Nebula Core: open source or source-available
NebulaOS: potentially partially open source
Nebula online services/backend: private
Branding assets: protected
```

---

## Core Vision

Nebula Browser is built around one simple idea:

> The web should be comfortable to use from a couch, handheld, controller, or gaming-focused operating system.

Most browsers are designed around keyboards, mice, and desktop workflows. Nebula Browser is designed around a different experience.

Nebula Browser aims to support:

* Controller-first navigation
* Large, readable interface elements
* Gamepad-friendly menus and shortcuts
* Steam Deck and handheld PC usability
* Custom home pages and launcher-style navigation
* Better integration with gaming-focused environments
* A UI that feels closer to a console app than a traditional browser

---

## Planned Features

The CEF rebuild is still early, but the intended feature set includes:

* Fully custom browser chrome
* Custom tab bar
* Custom address bar
* Controller-friendly navigation system
* On-screen keyboard support
* Bookmarks and quick access tiles
* Custom home page
* Desktop mode and controller mode
* Fullscreen and couch-friendly browsing
* Custom context menus
* Download management
* Settings system
* Theme support
* Steam Deck / handheld-focused layout options
* Optional integration with NebulaOS

Some features may be ported or adapted from the earlier Electron version where it makes sense.

---

## Nebula Core

Nebula Browser may also make use of **Nebula Core**, a shared package system for Nebula-related projects.

Nebula Core is intended to provide reusable systems such as:

* Controller input handling
* Navigation helpers
* UI components
* Glyphs and button prompts
* Theming utilities
* On-screen keyboard systems
* Shared Nebula project logic

Because the new browser is CEF-based rather than Electron-based, some Nebula Core modules may need to be adapted for native integration or used through the browser UI layer rather than directly inside the C++ application layer.

---

## Relationship to NebulaOS

Nebula Browser is part of the wider **Nebula Project** ecosystem.

NebulaOS is a controller-focused operating system / shell concept designed around gaming, handheld PCs, and living room devices. Nebula Browser is intended to become one of the key built-in applications for that ecosystem.

While Nebula Browser can exist as a standalone app, its long-term role is to provide a web browsing experience that feels native inside NebulaOS.

---

## Technology Stack

The current redevelopment direction uses:

* **C++** for the native browser shell
* **CEF / Chromium Embedded Framework** for web rendering
* **HTML, CSS, and JavaScript** for the custom browser UI
* **CMake** for project configuration
* Platform-specific CEF builds for Windows, Linux, and macOS

The previous version used Electron, but the current direction is focused on CEF.

---

## Repository Structure

The repository structure may change as the CEF rebuild develops, but the project is expected to follow a layout similar to:

```text
NebulaBrowser/
├── app/
│   ├── browser/
│   ├── ui/
│   └── platform/
├── cef/
│   └── README.md
├── resources/
│   ├── icons/
│   ├── themes/
│   └── pages/
├── scripts/
├── third_party/
├── CMakeLists.txt
└── README.md
```

CEF itself should generally not be committed directly into the repository. Instead, platform-specific CEF builds should be downloaded separately and placed into the expected local folder during development.

---

## Development Notes

Nebula Browser is still experimental and in transition from the original Electron version to the new CEF version.

Expect changes in:

* Build setup
* File structure
* UI architecture
* Controller input systems
* Platform support
* Packaging and distribution

The project is not yet considered production-ready.

---

## Building

Build instructions will be expanded as the CEF version becomes more stable.

At a high level, development requires:

1. A supported C++ compiler
2. CMake
3. A downloaded CEF binary distribution for your operating system
4. The Nebula Browser source code
5. A local build folder

Example build flow:

```bash
cmake -S . -B build
cmake --build build
```

More detailed platform-specific instructions will be added later. For macOS/Linux prerequisites, directory structure, and current port status, see [docs/cross-platform.md](docs/cross-platform.md).

### Quick start (Windows)

1. Download the [CEF standard binary distribution](https://cef-builds.spotifycdn.com/index.html) for Windows 64-bit.
2. Extract into `thirdparty/cef/` so `thirdparty/cef/cmake/FindCEF.cmake` exists.
3. Build:

   ```powershell
   cmake -B build
   cmake --build build --config Release
   ```

4. Run `build\Release\NebulaBrowser.exe`.

---

## Platform Goals

Nebula Browser is being designed with cross-platform support in mind.

Target platforms include:

* Windows
* Linux
* SteamOS / Steam Deck
* macOS

Linux and SteamOS are especially important because Nebula Browser was originally created to solve a gap in the handheld and couch-browsing experience.

---

## Release Plans

Nebula Browser does not currently have a fixed release date.

The current release plan is:

1. Rebuild the core browser shell in CEF
2. Replace default Chromium UI with a custom Nebula interface
3. Restore core browser functionality
4. Add controller-first navigation
5. Prepare an early public build
6. Release through Itch.io first
7. Explore additional distribution options later

---

## Contributing

Nebula Browser is currently a personal / experimental project and is still finding its new technical direction.

Contributions, ideas, testing, and feedback may become more open once the CEF rebuild has a more stable foundation.

---

## Licensing

Nebula Browser separates source code licensing from branding and visual assets.

The preferred licensing direction is:

* **Source code:** Mozilla Public License 2.0
* **Documentation:** CC BY 4.0, unless otherwise specified
* **Branding, logos, icons, names, and visual identity:** All Rights Reserved, unless explicitly stated otherwise

MPL-2.0 is being used because it is a practical open-source license for a browser-style project. It allows people to use, modify, and contribute to the code while requiring changes to MPL-licensed files to remain open under the same license.

The Nebula name, logo, and brand identity are not automatically granted for reuse just because the code is open source. See [ASSETS-LICENSE.md](ASSETS-LICENSE.md) for the separate asset and branding terms.

See the CEF distribution and Chromium license terms for third-party runtime components.

---

## About

Nebula Browser is created as part of the wider Nebula Project, a collection of software experiments focused on controller-first computing, gaming-focused interfaces, and custom desktop experiences.

The project began as a browser for SteamOS and Steam Deck users, but is now evolving into a more flexible, native, and custom browser experience built around CEF.
