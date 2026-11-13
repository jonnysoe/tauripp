# tao++ GUI: macOS

## Important boundary

SwiftUI is a Swift framework, not a C++ GUI library. The macOS adapter must therefore be a Swift target, or an Objective-C++/Swift target, that links the C++ static core. The C++ core must not include SwiftUI, AppKit, or WebKit headers.

SwiftUI provides views, controls, scenes, menus, commands, state propagation, accessibility, drag/drop, focus, and system-event handling. Use `App` and window scenes for the application shell. Use AppKit only where a system service is not exposed adequately by SwiftUI.

## C++ and Swift bridge

Requirements:

- macOS deployment target chosen for the application, with SwiftUI available from macOS 10.15;
- Xcode/Swift toolchain and C++17-compatible `libc++` build;
- C++ interoperability enabled for Swift 5.9+ if direct C++ imports are used;
- a Clang module with an umbrella header, or an Xcode bridging header, for the tao++ public headers.

Prefer a narrow C ABI even though Swift can import many C++ APIs directly. Expose fixed-width POD structs, UTF-8 spans, opaque IDs, and callback registration functions. Keep ownership in C++ with explicit `retain/release` or `destroy` operations and dispatch every UI operation to Swift's main actor.

A Swift wrapper can import the C++ module and map core state to SwiftUI view state. A C callback with an opaque context is the simplest C++-to-Swift event path; unregister it before the Swift owner deinitializes. If C++ must call Swift, Xcode can provide the generated `Module-Swift.h` header, but generated Swift interfaces are more sensitive to toolchain and supported-type changes.

Avoid C++ exceptions across Swift, references whose lifetime depends on a C++ object, C++ iterators, and public `std::variant`-style types at the bridge. Swift cannot catch a C++ exception that escapes into Swift. C++ classes imported as Swift reference types also have a higher minimum deployment version (currently macOS 13.3); a C ABI of opaque handles avoids that constraint and keeps a macOS 10.15-compatible design possible.

References: [Swift/C++ interop](https://www.swift.org/documentation/cxx-interop/), [mixed-language build setup](https://www.swift.org/documentation/cxx-interop/project-build-setup), and [interop constraints](https://www.swift.org/documentation/cxx-interop/status).

## Native view and webview hosting

Use [AppKit integration](https://developer.apple.com/documentation/swiftui/appkit-integration) in either direction:

- `NSHostingView` or `NSHostingController` embeds SwiftUI in an AppKit host;
- `NSViewRepresentable` embeds an AppKit view in SwiftUI and provides `makeNSView`, `updateNSView`, an optional coordinator, and teardown;
- `WKWebView` is the system webview for macOS. Configure `WKWebViewConfiguration`, `WKUserContentController`, navigation/UI delegates, message handlers, and custom URL schemes in the adapter.

SwiftUI owns the layout of an `NSViewRepresentable` view. Do not set its frame or bounds from C++; publish desired state to Swift and let SwiftUI update the view. Use `WKWebView` document-start user scripts and `WKScriptMessageHandler` for the tauri++ bridge, then use `evaluateJavaScript` for host-to-page calls.

The webview adapter should cover navigation decisions, page start/finish, new-window requests, downloads, permissions, cookies/data stores, process termination, devtools/inspectability, and drag/drop as capabilities. The common IPC contract is in [tauripp.md](tauripp.md).

## Tao-compatible native capabilities

The vendored Tao macOS backend exposes `NSWindow` and `NSView` handles, window options, fullscreen, shadow, title-bar/traffic-light controls, tabbing, document-edited state, dock badges, activation policy, reopen events, monitors, scale factor, and application show/hide. tao++ should expose equivalent capability-neutral operations and let the Swift adapter map them to `NSWindow`, `NSApplication`, SwiftUI scenes, or AppKit.

A status item/tray, global menu, document handling, and application activation may require AppKit even when the main content is SwiftUI. Keep those as optional adapter capabilities; do not force AppKit types into the portable API.

## Local references

- [Tao macOS extension traits](../third_party/tao/src/platform/macos.rs)
- [Tao macOS backend](../third_party/tao/src/platform_impl/macos/mod.rs)
- [Tauri runtime webview traits](../third_party/tauri/crates/tauri-runtime/src/lib.rs)
- [Tauri WRY macOS integration](../third_party/tauri/crates/tauri-runtime-wry/src/lib.rs)
