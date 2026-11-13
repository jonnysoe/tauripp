# Tauri++

A C++17 native host for the small Tauri-compatible hello/buttons slice described in
[`docs/tauripp.md`](docs/tauripp.md), with the portable window/event layer described in
[`docs/taopp.md`](docs/taopp.md).

## Current implementation

- `taopp_core`: OS-neutral IDs, window state, events, posted UI work, and run-loop control.
- `taopp_host`: versioned C ABI plus the platform native facade.
- Windows: Win32 windows with a C++/WinRT XAML Island overlay, compact modern rounded controls, stable hover states, live system light/dark and accent updates, toggle switches, DPI events, close veto, and UI-thread wakeups; the basic Win32 fallback is used only when XAML is unavailable at configure time.
- Other operating systems: compiling stubs that return `unsupported`.
- `tauripp_core`: command registry, synchronized counter state, Tauri-shaped JSON invoke envelopes, callback cleanup, and a secured local asset store.
- `tauripp`: reusable static framework library exposing the application, command, IPC, and native-host facade.
- `tauripp_hello`, `tauripp_buttons`, and `tauripp_hello_buttons`: native Windows GUI examples that link to `tauripp` and are the launchable programs.

The WebView2 bridge, Linux adapters, macOS Swift adapter, plugins, and optional high-throughput JSON parser remain follow-up work. No Rust or Node.js runtime is required for this slice.

## Build

From a Windows developer PowerShell or command prompt:

```text
compile.cmd
```

The script prefers the matching Clang pair when available, uses Ninja Multi-Config, and builds Release. `compile.ps1` forwards to the same script.

With CMake Tools, build the `tauripp` framework target or one of the example targets. The launchers are written under `build/bin` for multi-config builds. Enable `TAURIPP_FETCH_YYJSON` only when the optional future parser dependency is wanted; it is fetched into `third_party` through `cmake/git_clone.cmake`.

## Tests

The CTest cases live in [`tests/CMakeLists.txt`](tests/CMakeLists.txt) and cover the portable event loop, exact hello response, app-scoped counter behavior, IPC callback/key handling, asset traversal/range handling, and the backend C ABI. The backend smoke test creates a hidden Win32 window on Windows and accepts the documented unsupported result on other platforms.