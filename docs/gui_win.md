# tao++ GUI: Windows

## Required foundation

Use a normal Unicode Win32 desktop host with C++17, the Windows SDK, and a per-monitor-DPI-aware manifest. Register a window class, create an `HWND` with `CreateWindowExW`, route messages through a `WNDPROC`, and run `GetMessageW` / `TranslateMessage` / `DispatchMessageW`. Window and WebView2 objects remain on the thread that owns their message queue.

The adapter should normalize `WM_SIZE`, `WM_MOVE`, `WM_CLOSE`, `WM_DESTROY`, focus, DPI, keyboard, pointer, wheel, touch, IME, and file-drop messages into the tao++ event records. Use `DefWindowProcW` for messages not owned by the adapter. Set process DPI awareness before creating windows and report logical versus physical dimensions consistently.

Typical native link requirements are User32, GDI32, Shell32, Ole32/Combase, and any optional DWM/Common Controls libraries. Keep these links in the Windows adapter, never in `taopp_core`.

## Optional C++ wrappers

### WTL

[WTL](https://github.com/Win32-WTL/WTL) is a mostly templated, small Win32/ATL library for frame, dialog, popup, standard/common controls, dialogs, property sheets, GDI, command bars, and resizing. It is designed to mix with SDK code and has little runtime overhead. The official project is mirrored from [SourceForge](https://sourceforge.net/projects/wtl/); its sources use the Microsoft Public License (MS-PL).

Use WTL inside the adapter for message maps, frame/dialog classes, control lifetime, and layout. Keep WTL headers and ATL types private to that target. tao++ must still own the normalized event and callback contract, because WTL is Windows-only and its template types are not a stable plugin ABI.

### WinLamb

[WinLamb](https://github.com/rodrigocfd/winlamb) is a header-only MIT C++11+ wrapper around Win32 message handling. It provides `window_main`, `window_control`, dialog classes, native control wrappers, menus, common dialogs, COM helpers, taskbar progress, and lambda-based message crackers.

Use it when lambda message registration and lightweight native controls are more useful than WTL's class model. It has been tested with Visual C++ 2017; treat it as an implementation dependency and keep its types out of public tao++ headers.

Bare Win32 remains the reference implementation because it gives the smallest dependency surface and makes message ordering, DPI, focus, and native-handle ownership explicit.

## Hybrid XAML Islands

The [UWP XAML hosting API](https://learn.microsoft.com/en-us/windows/uwp/xaml-islands/host-custom-control-with-xaml-islands-cpp) can attach C++/WinRT XAML content to an existing Win32 `HWND` through `DesktopWindowXamlSource` and `IDesktopWindowXamlSourceNative`. taopp uses this as the preferred Windows control backend when the installed SDK exposes the required projection. It overlays the existing Win32 parent and does not replace the Win32 event loop. If the SDK is unavailable at configure time, the adapter builds its basic Win32 fallback; if XAML is compiled in but cannot activate at runtime, window creation fails with the activation error instead of silently regressing the UI.

Requirements from Microsoft's C++ walkthrough:

- Windows 10 version 1903 SDK or later;
- Visual Studio 2019 16.4.3 or later in the documented setup, or an equivalent current C++/WinRT toolchain;
- C++/WinRT projection and Windows Runtime metadata;
- a XAML control project, normally built as a DLL, plus the desktop host;
- MSIX packaging or a suitable application manifest; unpackaged deployment also needs the Visual C++ runtime;
- one XAML application initialization path per XAML host arrangement and a single-threaded apartment on the UI thread.

Adapter sequence:

1. Initialize the Win32 UI thread and C++/WinRT apartment.
2. Create the XAML application and `DesktopWindowXamlSource`.
3. Attach the source to the parent `HWND` and set its `Content`.
4. Resize the island's child `HWND` on `WM_SIZE` and forward focus/keyboard transitions.
5. Close the source before destroying the parent window.

The current examples use the island for a light Windows 11-inspired surface: a spacious card, rounded controls, a blue primary button, larger typography, and `ToggleSwitch` for native pill-style state changes. In XAML terminology, the visible switch parts are the **track** (its fill and stroke) and the **thumb** (the moving ball; its movement is exposed as a knob offset in template settings). The adapter keeps `UISettings::ColorValuesChanged` registered for the XAML thread and reapplies the root, card, control, accent, and title-bar colors in place when the system theme or accent changes. Toggle states use the system disabled color for the off-state track stroke and thumb, system button-face color for the off-state track fill, system accent for the on-state track stroke/fill, and a light-theme white or dark-theme black on-state thumb. XAML objects remain private to the Windows adapter; the common control model carries only IDs, bounds, text, and style intent.

Do not confuse UWP XAML Islands with a generic WinUI 3 host. New WinUI 3/Windows App SDK controls have a different deployment and window model. Use a separate adapter if that path is needed. XAML roots, not `CoreWindow` or `ApplicationView`, are the correct context for hosted content.

## WebView2

WebView2 is the Windows webview for the tauri++ adapter. The native sequence is asynchronous:

1. Add the WebView2 SDK (`Microsoft.Web.WebView2`) and include `WebView2.h`.
2. Initialize COM on the UI thread.
3. Call `CreateCoreWebView2EnvironmentWithOptions` when a custom browser folder, user-data folder, or browser flags are needed.
4. In the completion callback call `ICoreWebView2Environment::CreateCoreWebView2Controller(HWND, ...)`.
5. Obtain `ICoreWebView2` from the controller, set settings and bounds, then navigate or load the local app URL.
6. Resize controller bounds on `WM_SIZE` and close the controller before the parent `HWND` is destroyed.

The essential bridge APIs are:

- `AddScriptToExecuteOnDocumentCreated` for the document-start bootstrap;
- `ExecuteScript` for host-to-page evaluation;
- `window.chrome.webview.postMessage` and `ICoreWebView2::PostWebMessageAsString/Json` for bidirectional messages;
- `WebMessageReceived` for page-to-host IPC;
- `NavigationStarting` to allow or cancel navigation;
- custom scheme/web-resource handlers when the local asset protocol is needed;
- download, permission, new-window, title, focus, process-failure, cookie, and devtools events as optional capabilities.

Use WRL or WIL for COM ownership and error handling if desired. WIL is header-only; it does not belong in the portable core.

Deployment is part of the adapter contract. Production apps use the WebView2 Runtime, not the installed Microsoft Edge Stable browser. Evergreen Runtime is shared and updated; Fixed Version is self-contained but adds more than 250 MB and must be updated by the app. Ship the architecture-matching `WebView2Loader.dll` or statically link `WebView2Loader.lib`, and check for an installed Runtime before creating the environment. A WebView2 environment owns a browser process group and a user-data folder; use one shared environment for webviews that should share profile data and process resources.

The WebView2 controller and COM objects are framework-owned opaque handles. They never cross the tao++ ABI. See Microsoft's [Win32 getting-started guide](https://learn.microsoft.com/en-us/microsoft-edge/webview2/get-started/win32), [C++ reference](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/), [distribution guide](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution), and [process model](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/process-model).

## Local Tao references

- [Tao Windows extension traits](../third_party/tao/src/platform/windows.rs)
- [Tao Win32 backend](../third_party/tao/src/platform_impl/windows/mod.rs)
- [Tauri WRY Windows integration](../third_party/tauri/crates/tauri-runtime-wry/src/lib.rs)
