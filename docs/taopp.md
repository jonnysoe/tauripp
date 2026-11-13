# tao++

Research and implementation plan for a native C++ counterpart to the vendored [Tao 0.37.0](../third_party/tao/README.md).

## Scope

Tao is a cross-platform window and event-loop library. It does **not** draw controls, host a webview, provide CSS, or provide a general menu, tray, or dialog framework. Tauri adds those layers through WRY and its runtime. tao++ should preserve that boundary.

"Interchangeable" means a compatible window/event capability model and native-handle semantics. It does not mean Rust ABI compatibility or binary compatibility with Tao.

The portable library should model controls as IDs, properties, actions, layout data, and events. The OS wrapper creates the actual `HWND`, GTK widget, Qt object, AppKit view, or SwiftUI view. No such native type belongs in the static library.

Platform-specific requirements are in:

- [Windows: Win32, WTL, WinLamb, XAML Islands, and WebView2](gui_win.md)
- [Linux: GTK4, Qt6, and runtime backend loading](gui_linux.md)
- [macOS: SwiftUI, AppKit, and C++ interop](gui_macos.md)

## Tao surface to emulate

| Tao | tao++ equivalent | Notes |
| --- | --- | --- |
| `EventLoopBuilder` / `EventLoop` | `App` / `EventLoop` | One GUI context; normally created and run on the UI thread. |
| `ControlFlow` | `RunMode` | `Poll`, `Wait`, timed wait, and exit with code. |
| `EventLoopProxy::send_event` | `post()` / `wake()` | Thread-safe queue into the GUI thread. |
| `WindowBuilder` / `WindowAttributes` | `WindowOptions` | Size, position, constraints, title, visibility, focus, decoration, fullscreen, theme, transparency, icon, and z-order. |
| `Window` / `WindowId` | `Window` / `WindowId` | Stable ID plus an opaque borrowed native handle. |
| `Event` / `WindowEvent` | `Event` / `WindowEvent` | Tagged records with explicit version and payload size. |
| `MonitorHandle` / `VideoMode` | `MonitorInfo` / `VideoMode` | Physical geometry, work area, scale factor, refresh rate, and bit depth where available. |
| `raw-window-handle` | `NativeWindowHandle` | Tagged, borrowed, lifetime-bound handle; never expose framework objects. |
| `Icon::from_rgba` | `ImageView` / `Icon` | Validate width, height, overflow, and RGBA byte count before ownership transfer. |

The Tao event ordering is useful: start of batch, window/device/user events, main-events-cleared, redraw requests, redraw-events-cleared, then loop destruction. Window events include resize, move, close, destroy, focus, scale change, theme, keyboard, modifiers, pointer, wheel, touch, IME, and file drag/drop. Keyboard data should retain both physical key code and logical key/text; Tao follows W3C-style key names.

Redraw is a request, not a rendering API. tao++ should expose `request_redraw()` and a redraw event, but leave rendering to the wrapper or another library. `run_return()` exists in Tao but is explicitly not a perfect `poll_events()` replacement; expose it only as an embedding option and document resize/modal-loop caveats.

## Static core and native wrapper

Build three conceptual layers:

1. `taopp_core` is a C++17 static library with no GUI headers or GUI link dependencies. It owns IDs, normalized data, command queues, callback registration, control state, errors, and lifetime rules.
2. `taopp_host` is the common wrapper contract. It translates core requests into native calls and translates native callbacks into `taopp_event` records. It can be implemented in C++ but its module boundary should be a versioned C ABI.
3. A platform adapter owns the event loop, native windows, controls, menus, dialogs, tray, and optional webview. See the platform documents linked above.

Use an entry point shaped like this at a dynamically loaded boundary:

```cpp
extern "C" const taopp_backend_api_v1 *taopp_backend_query(uint32_t requested_abi);
```

The API table should contain `abi_version`, `struct_size`, capability bits, lifecycle functions, `run`, optional `run_iteration`, `post`, window creation/destruction, and callback registration. Pass only fixed-width integers, UTF-8 byte spans, POD structs, opaque IDs, and function pointers with an opaque context. Do not pass `std::string`, STL containers, exceptions, RTTI objects, framework pointers, or allocator-owned memory across the boundary.

Rules for both directions:

- Native UI operations and callbacks run on the adapter's UI thread.
- Worker threads use `post()`; they never touch a native object directly.
- Callback registration returns an ID and has an explicit unregister operation. Unregister before destroying its context.
- The adapter owns native handles. Core handles are borrowed and invalid after a destroy event.
- Every callback that can veto an operation returns a result. In particular, close-requested must support allow/prevent even though Tao itself only reports the request.
- Errors are returned as stable error codes plus an optional owned UTF-8 message. No exception crosses the ABI.
- The application owns the top-level loop. A backend may either run it or integrate one iteration when the framework supports it.

A C++ convenience facade may wrap this table for applications, but the C ABI is the future-proof point for Swift and `dlopen` adapters.

## CMake implementation

Initially select exactly one Linux GUI implementation at configure time:

```text
TAOPP_LINUX_GUI = GTK4 | QT6
```

The CMake target graph should be equivalent to:

```text
taopp_core (STATIC)   <- no GUI dependencies
taopp_host (STATIC)   <- taopp_core and backend ABI headers
taopp_gui_<platform>  <- native framework adapter
taopp_app             <- taopp_host + one selected adapter
```

For the later runtime switch, build `taopp_gui_gtk4.so` and `taopp_gui_qt6.so` as separate modules. The host loads one trusted module with `dlopen`, verifies `taopp_backend_query`, ABI version, `struct_size`, and capability bits, then keeps the module loaded until all objects and callbacks are gone. Never search the current working directory for an arbitrary backend.

Loading a module at runtime removes the host's direct startup dependency; it does not remove the adapter's dependency on its toolkit. Keep GTK/Qt types and their ABI entirely inside their adapter. The loader design and toolkit-specific caveats are in [gui_linux.md](gui_linux.md).

## Compatibility checklist

A first useful Tao-compatible slice is:

- one application context and one or more windows;
- run, exit, wake, and user events;
- window options and runtime state operations;
- close, resize, move, focus, scale, theme, keyboard, pointer, touch, IME, and file-drop events;
- monitor enumeration and logical/physical pixel conversion;
- native handle access with lifetime documentation;
- icon validation and redraw scheduling.

Keep these as explicit capability bits because Tao documents platform gaps: Wayland has no global coordinate system; Linux monitor video modes are unavailable; some Linux window operations are asynchronous; content protection and z-order are not universal; macOS and mobile operations often require the main thread; and Windows transparency may require application rendering.

Menus, tray, native dialogs, webviews, accessibility, and rendering should be optional adapter capabilities rather than silently emulated in core. Tauri-compatible webview and application services belong in [tauripp.md](tauripp.md).

## Local sources

- [Tao module overview](../third_party/tao/src/lib.rs)
- [Event loop and proxy](../third_party/tao/src/event_loop.rs)
- [Events](../third_party/tao/src/event.rs)
- [Window builder and operations](../third_party/tao/src/window.rs)
- [Monitor and DPI types](../third_party/tao/src/monitor.rs)
- [Raw handles and features](../third_party/tao/Cargo.toml)
- [Tao platform extensions](../third_party/tao/src/platform)
- [Tauri architecture](../third_party/tauri/ARCHITECTURE.md)
