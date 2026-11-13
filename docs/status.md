# Implementation Status

Updated 2026-09-04.

- Implemented `taopp_core`, the versioned `taopp_backend_api_v1`, and a Windows Win32 adapter with a C++/WinRT XAML Island overlay, native controls, DPI events, close veto, and UI-thread wakeups.
- XAML examples now follow the Windows light/dark preference through `UISettings`, use compact DIP geometry, and override pointer-over/pressed Button resources so controls remain visible.
- The Windows adapter listens for `UISettings::ColorValuesChanged`, refreshes live XAML controls and DWM title bars, and unregisters the listener during teardown.
- ToggleSwitch styling now distinguishes the track fill/stroke and thumb, uses system disabled/button-face colors for off, and uses a theme-contrast thumb for on; toggle headers are explicit themed TextBlocks.
- Implemented `tauripp_core` commands/state, Tauri-shaped invoke parsing with callback cleanup, secure local assets, and hello/hello-buttons fixtures.
- Made `tauripp` a reusable static framework library and moved the native GUI fixtures into `tauripp_hello`, `tauripp_buttons`, and `tauripp_hello_buttons`; other OSes compile explicit unsupported stubs.
- Moved all CTest registration and PATH runtime setup to `tests/CMakeLists.txt`.
- Verified Ninja Multi-Config with Clang 21.1.8 and all seven core/IPC/assets/ABI tests.
- Deferred: WebView2 transport, Linux GTK4/Qt6 adapters, macOS Swift adapter, plugins, and production bundling. Windows only uses basic Win32 controls when XAML is unavailable at configure time; runtime XAML activation errors are reported.