# tao++ GUI: Linux

## Current repository fact

The vendored Tao and Tauri WRY code currently use GTK3 on Linux. Tao's manifest depends on `gtk = 0.18`; Tauri WRY uses GTK3 plus WebKitGTK 4.1. tao++'s requested GTK4 option is therefore a new adapter and cannot reuse those GTK3 widget types unchanged.

## GTK4 adapter

Requirements:

- C++17 compiler and CMake;
- GTK4 development headers and libraries, installed with `pkg-config` metadata for `gtk4`;
- GLib/GObject/GIO, GDK/GSK, Pango, and the selected X11 or Wayland runtime;
- GTK4 is LGPL-2.1-or-later, so retain its notices and verify the application's distribution model.

Use `GtkApplication` for a normal application, or `gtk_init_check()` plus a controlled `GMainContext` when embedding into an existing host. The GTK main context must be iterated by one UI thread. GTK widgets are not thread-safe. Worker threads return results through `g_main_context_invoke()` or an attached source; `g_main_context_wakeup()` is useful when a worker posts work to a sleeping loop.

Map GTK signals and GDK events into tao++ records. Keep `GtkWindow`, `GtkWidget`, `GdkSurface`, and GObject reference ownership inside the adapter. GTK4 provides native controls, CSS styling, actions, menus, dialogs, accessibility, and drag/drop, but only expose them through capability-neutral core IDs and properties.

Use GTK's backend-neutral GDK APIs first. X11-only operations such as global coordinates, native XIDs, or window-manager hints must be capability-checked. Wayland has no global coordinate system; return an explicit unsupported result instead of inventing coordinates.

References: [GTK4 API](https://docs.gtk.org/gtk4/), [GTK initialization](https://docs.gtk.org/gtk4/initialization.html), [GTK threading](https://docs.gtk.org/gtk4/section-threading.html), and [GLib main loop](https://docs.gtk.org/glib/main-loop.html).

## Qt6 adapter

Use Qt6 Core plus Gui/Widgets for a native widget implementation. Add WebEngine only when the Qt adapter must host its own Chromium webview. Requirements are a Qt6 development installation, CMake package files, the matching compiler ABI, and runtime Qt libraries/plugins.

There must be exactly one `QApplication` for a Widgets process. Construct it before GUI objects and call `exec()` on the UI thread. From worker threads use `QCoreApplication::postEvent`, queued signal/slot connections, or a queued invocation; do not access `QWidget`, `QWindow`, or event-driven objects from another thread. Keep `QObject` parent/child lifetimes in the owning thread.

Map Qt events to the same tao++ records and keep `QObject`, `QWidget`, `QWindow`, `QMenu`, and `QWebEngineView` private to the adapter. Use `QAbstractNativeEventFilter` only for unavoidable native messages; prefer Qt events for portable behavior.

Qt WebEngine is Chromium-based, multi-process, GPU-backed, and provides `QWebEngineView`, profiles, cookies, script injection, URL scheme handlers, request interception, and WebChannel. It is much heavier than a system WebKit webview. For a performance-first build, make it optional and avoid pulling it into the core or the GTK4 configuration.

References: [QApplication](https://doc.qt.io/qt-6/qapplication.html), [QCoreApplication](https://doc.qt.io/qt-6/qcoreapplication.html), [threads and QObjects](https://doc.qt.io/qt-6/threads-qobject.html), and [Qt WebEngine](https://doc.qt.io/qt-6/qtwebengine-overview.html). Check the selected Qt modules' LGPL/GPL/commercial license terms before distribution.

## Compile-time selection, then runtime loading

Initially expose a CMake cache choice such as:

```text
TAOPP_LINUX_GUI = GTK4 | QT6
```

Compile only the selected adapter into the first application. For future runtime selection, build two modules:

```text
libtaopp_gui_gtk4.so
libtaopp_gui_qt6.so
```

The host uses POSIX `dlopen` / `dlsym` (or GLib `GModule`) to load a trusted module and calls a versioned `extern "C" taopp_backend_query` function. Check ABI version, table size, pointer validity, and capability bits before use. Use `RTLD_NOW | RTLD_LOCAL`, a known absolute module directory, and keep the module loaded until every callback, widget, and queued task is destroyed.

Do not make the common loader resolve Qt C++ class methods by mangled symbol names. Qt's `QLibrary` documentation requires exported C functions for `resolve`; a Qt adapter should instead be compiled against Qt and export only the tao++ C entry table. `QPluginLoader` is useful for Qt-native plugins, but its QObject/Qt metadata ABI is not the common tao++ ABI. GTK has a C ABI and can technically be resolved function-by-function, but a linked GTK adapter is simpler and less fragile; the adapter module is still loaded at runtime by the host.

The result is runtime-selectable GUI behavior without direct startup linkage from the host to either toolkit. It does not make GTK and Qt objects interchangeable.

## Webview note for tauri++

The selected GUI toolkit and the webview toolkit must agree on the host widget model. The current vendored Tauri WebKitGTK integration is GTK3/WebKitGTK 4.1. A GTK4 adapter must use a WebKitGTK API/runtime that actually supports GTK4, or isolate the webview in a separate adapter/process. A Qt6 adapter should use Qt WebEngine or another deliberately bridged webview; do not cast a GTK widget into a Qt hierarchy.

The webview IPC and asset contract is specified in [tauripp.md](tauripp.md).

## Local references

- [Tao Linux dependencies](../third_party/tao/Cargo.toml)
- [Tao Unix extension traits](../third_party/tao/src/platform/unix.rs)
- [Tao GTK3 backend](../third_party/tao/src/platform_impl/linux/mod.rs)
- [Tauri WRY Linux dependencies](../third_party/tauri/crates/tauri-runtime-wry/Cargo.toml)
