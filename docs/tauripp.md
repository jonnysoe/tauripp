# tauri++

Research and implementation plan for a native C++ counterpart to the vendored Tauri 2 workspace. The local versions are Tauri `2.11.5`, `tauri-runtime-wry` `2.11.4`, and `@tauri-apps/api` `2.11.1`.

## Compatibility goal

Interchangeability is a frontend contract: the same JavaScript/TypeScript application should be able to call the same module paths, method names, event names, payload shapes, and promise behavior. It is not Rust ABI compatibility.

Tauri's deployed application does not ship a Node runtime. TypeScript, JavaScript, HTML, CSS, and WASM are frontend assets. tauri++ should keep that model: the C++ process owns native services and a system webview renders the frontend.

## Architecture

```text
frontend assets (HTML/CSS/JS/TS/WASM)
        |
@tauri-apps/api-compatible package or window.__TAURI__
        |
webview bridge: invoke, callbacks, channels, events
        |
tauripp_core (C++17 static): commands, events, state, resources, ACL, assets
        |
taopp + OS webview adapter + native GUI adapter
```

Split the implementation into:

- `tauripp_core`: no GUI or webview headers; owns command registry, event broker, app/window/webview IDs, state, resource handles, permission evaluation, asset routing, and queues.
- `tauripp_host`: connects the core to [taopp](taopp.md) and a webview adapter. Use the same versioned C ABI rules as tao++.
- OS adapters: WebView2 on Windows, a matching WebKitGTK or Qt webview on Linux, and WKWebView through the Swift macOS adapter. See [Windows](gui_win.md), [Linux](gui_linux.md), and [macOS](gui_macos.md).
- frontend package: preserve `@tauri-apps/api` imports and TypeScript declarations, changing only the internal transport when possible.

## Tauri API to preserve

The vendored [API index](../third_party/tauri/packages/api/src/index.ts) exports these modules:

| Module | First compatibility slice |
| --- | --- |
| `core` | `invoke`, `transformCallback`, `Channel`, `Resource`, `convertFileSrc`, serialization hook. |
| `event` | `listen`, `once`, `unlisten`, `emit`, `emitTo`, event IDs, and targets. |
| `window` | Current/all/focused windows, creation, close/destroy, title, visibility, focus, size/position, constraints, fullscreen, maximize/minimize, decoration, theme, cursor, DPI, monitor, drag/drop, and attention. |
| `webview` | Current/all webviews, creation, bounds, URL, navigation/reload, script evaluation, focus/show/hide, zoom/background, cookies, drag/drop, and webview events. |
| `webviewWindow` | A window and webview combined under one label. |
| `dpi` | Logical/physical positions and sizes with scale conversion. |
| `app` | Name/version/identifier, theme, show/hide, bundle type, and data-store hooks where supported. |
| `path` | Standard user/app/resource/temp directory resolution and path join/normalize/resolve. |
| `image` | RGBA images and image resources for menus, windows, and tray. |
| `menu` / `tray` | Native menu trees, menu items/actions, application/window menus, tray icons, menus, titles, tooltips, visibility, and tray events. |
| optional plugins | Dialog, filesystem, notification, process, shell/opener, HTTP, SQL, store, updater, deep link, shortcut, and other platform features. |

The package supports both ESM/CJS builds and a global `window.__TAURI__` mode when global APIs are enabled. A minimal tauri++ runtime should implement `window.__TAURI_INTERNALS__` plus metadata for the current window and webview; the package can then remain almost unchanged.

## Webview lifecycle and host hooks

For every webview, preserve:

1. create a labeled webview inside a labeled window;
2. load an embedded app URL, local route, data URL when explicitly allowed, or remote URL;
3. inject the bridge and plugin initialization scripts at document start, before page scripts;
4. inject current window/webview labels and origin metadata;
5. handle page start/finish, navigation allow/deny, new-window requests, title changes, downloads, permissions, process failure/termination, drag/drop, cookies, and devtools as capabilities;
6. close the webview before its parent native window and unregister all callbacks.

Tauri separates a window from its webview and permits multiple webviews per window. `WebviewWindow` combines them for the common one-window/one-webview case. Preserve that distinction in core even if an adapter initially supports only one child.

## IPC wire contract

The public `invoke(cmd, args, options)` call returns a promise. The current Tauri bridge sends a logical envelope containing:

```json
{
  "cmd": "command-name",
  "callback": 12,
  "error": 13,
  "payload": {},
  "options": { "headers": {} },
  "__TAURI_INVOKE_KEY__": "runtime-key"
}
```

Implement the following behavior:

- `cmd` is a registered application command or a namespaced plugin command such as `plugin:menu|append`.
- `payload` is JSON for normal arguments. Desktop paths also support a raw byte body for `ArrayBuffer`/`Uint8Array` transfers.
- The command handler receives the requesting webview label, URL/origin, headers, body type, and resolved permissions.
- A successful result is JSON or raw bytes; an error is a JSON value delivered to the error callback. Resolve/reject the JS promise exactly once.
- Callback IDs are registered in the page runtime and explicitly unregistered. Never keep a callback after webview destruction.
- For custom-protocol transport, Tauri uses POST/OPTIONS on an `ipc` scheme and returns `Tauri-Response: ok` or `error`, with JSON or octet-stream content. A native post-message transport may use another physical channel, but it must preserve the logical envelope and response behavior.

### Channels and large data

Tauri serializes a JS `Channel` as `__CHANNEL__:<id>`. Messages carry monotonically increasing indexes; the frontend buffers out-of-order messages and receives an indexed end marker. Small messages can be evaluated directly; larger JSON/raw messages are fetched through a one-shot data ID. tauri++ should preserve ordering, cleanup, and binary transfer, while using a bounded queue and `ArrayBuffer` rather than JSON byte arrays for large data.

The channel mechanism is suitable for upload/download progress, streams, tray actions, and plugin callbacks. It is not a substitute for a general shared-memory transport.

## Events

Preserve `Event<T> { event, id, payload }` and the target kinds `Any`, `AnyLabel`, `App`, `Window`, `Webview`, and `WebviewWindow`. Implement `listen`, `once`, `unlisten`, backend `emit`, and targeted `emitTo` with explicit listener lifetime.

Keep the system names used by the API, including resize, move, close-requested, destroyed, focus/blur, scale-change, theme-changed, window-created, webview-created, suspended/resumed where supported, and drag-enter/over/drop/leave. Native events from taopp are translated once into these Tauri-shaped events. Close requests must allow a handler to prevent close.

## Command registry, state, and resources

Replace Rust `#[command]` plus `generate_handler!` with an explicit C++ registry:

```text
register("name", handler(request, responder))
register("plugin:name|command", handler)
```

Handlers may complete synchronously or retain a responder for asynchronous completion. Argument decoding should validate named JSON fields and return structured errors. Do not block the GUI thread on filesystem, network, compression, or CPU work.

Tauri managed state is app-lifetime, type-keyed, and synchronized. In C++, use an app-scoped registry keyed by a stable type/name ID and store `shared_ptr`-like ownership behind explicit locks or actor queues. Resources are numeric IDs backed by native objects and exposed to JS as `Resource`; implement `close`/release and automatic cleanup at app and webview destruction.

Use logical and physical DPI types at every window/webview boundary. Match Tauri's logical-pixel creation options and physical-pixel query results; keep scale-change events authoritative.

## Security model

The permission boundary is part of compatibility, not an optional hardening pass:

- identify the requesting webview, window, URL/origin, and frame;
- default commands to deny until a capability grants them;
- resolve allow/deny permissions and command/global scopes before dispatch;
- namespace plugin commands and validate plugin configuration;
- restrict navigation and new windows with an allow callback;
- serve local assets only from a safe, allowlisted root; canonicalize paths, reject traversal, set MIME types, and support range requests only when intended;
- apply CSP and avoid exposing unrestricted filesystem paths or arbitrary native evaluation;
- keep a per-runtime invoke key/token and reject malformed or stale requests;
- make remote origins less privileged than the packaged local origin;
- expose native permission prompts through explicit adapter callbacks.

Tauri's reference model is in [runtime authority](../third_party/tauri/crates/tauri/src/ipc/authority.rs), [IPC](../third_party/tauri/crates/tauri/src/ipc), [asset protocol](../third_party/tauri/crates/tauri/src/protocol/asset.rs), and the [permissions guide](https://v2.tauri.app/security/permissions/).

## Plugins

Implement static C++ plugins first. The minimum plugin lifecycle mirrors Tauri's `Plugin` trait:

- name and configuration;
- setup/initialize;
- document-start initialization script;
- window-created and webview-created;
- navigation and page-load callbacks;
- command/invoke handler;
- URI scheme/resource handler;
- application event hook;
- teardown.

A plugin may ship a frontend package with bindings that call `plugin:<name>|<command>`. Give every plugin a permission manifest and scope schema. A later optional plugin loader can use a versioned C ABI, but never pass C++ framework or STL types between a plugin DLL/SO and the core.

## Assets and frontend languages

Use a custom local protocol rather than a localhost server for production assets. The protocol must return HTML, CSS, JS, images, fonts, and WASM with correct MIME types, cache behavior, and CSP. `convertFileSrc` should produce an allowlisted asset URL, not a raw `file:` URL.

TypeScript is a development/build language; publish the generated ESM/CJS files and `.d.ts` declarations. WASM stays in the webview and communicates with C++ through the same invoke/event/channel APIs. Use raw byte channels for WASM or media data. No Node runtime is required after frontend assets are built.

## Basic fixtures and future CTest plan

The vendored Tauri examples are the behavioral reference. Do not run them from CTest: they are Rust examples and are not the tauri++ implementation under test.

### Tauri reference: hello world

[examples/helloworld/main.rs](../third_party/tauri/examples/helloworld/main.rs) defines:

```text
greet(name: &str) -> "Hello {name}, You have been greeted from Rust!"
Builder::default().invoke_handler(generate_handler![greet]).run(context)
```

[examples/helloworld/index.html](../third_party/tauri/examples/helloworld/index.html) has an `#name` input, a `Greet` submit button, and a `#message` paragraph. It destructures `window.__TAURI__.core.invoke`, prevents the form's default submit, invokes `greet` with `{ name }`, and writes the resolved string to the paragraph.

[examples/helloworld/tauri.conf.json](../third_party/tauri/examples/helloworld/tauri.conf.json) enables `withGlobalTauri`, loads `index.html`, creates an 800x600 resizable window titled `Welcome to Tauri!`, and permits the `ipc:` connection in CSP. Tauri injects the global API and `__TAURI_INTERNALS__` before application scripts; the global bundle's `invoke` registers success/error callbacks and sends the logical IPC envelope.

### Tauri reference: simple buttons

[examples/state/main.rs](../third_party/tauri/examples/state/main.rs) manages `Counter(Mutex<isize>)` and registers four commands: `get`, `increment`, `decrement`, and `reset`. Each command returns the resulting integer. [examples/state/index.html](../third_party/tauri/examples/state/index.html) calls `get` on `DOMContentLoaded`, then binds the three buttons `Increment`, `Decrement`, and `Reset` to the matching commands. Reloading the page does not reset the counter because the managed state belongs to the app, not the document.

For the requested combined fixture, start with the `helloworld` form and add the `state` counter heading and three buttons below it. Keep the greeting and counter commands independent so one fixture exercises both a named JSON argument and payload-free stateful calls.

This gives two intentionally small contracts:

| Fixture | Inputs | Observable result |
| --- | --- | --- |
| `hello` | submit `Ada` | `Hello Ada, You have been greeted from C++!` appears in `#message`. |
| `buttons` | initial load, increment, decrement, reset | displayed values are `0`, `1`, `0`, and `0`; the state survives a webview reload. |
| `hello-buttons` | submit `Ada`, then increment/decrement/reset | greeting appears in `#message` and the counter displays `0`, `1`, `0`, and `0`; greeting and state do not interfere. |

### tauri++ adaptation

Prefer the same HTML files and the same JavaScript calls. A tauri++ host should make `withGlobalTauri: true` equivalent inject `window.__TAURI__` and `window.__TAURI_INTERNALS__`; then the fixtures need no wrapper. If a frontend package is preferred, keep the upstream `@tauri-apps/api` source and provide only a transport-compatible `__TAURI_INTERNALS__`. A small `tauripp-compat.js` shim is the fallback when the global bundle cannot be shipped, not a change to fixture behavior.

Implement the `hello` fixture as follows:

1. Package `index.html` as an embedded local asset and create one window/webview labeled `main`.
2. Match the Tauri window config: title `Welcome to Tauri!`, logical size 800x600, resizable, not fullscreen, global API enabled, and CSP allowing the chosen IPC transport.
3. Register a command named exactly `greet`. Decode the required string field `name`, reject a missing or invalid field, and return the exact reference string so the frontend remains interchangeable.
4. Inject the bridge at document start, load the page, and resolve the callback exactly once when the C++ command completes.
5. Verify the form path through the real webview: set `#name`, submit the form, await the promise, and inspect `#message`.

Implement the `buttons` fixture with the same command names and payload-free calls. Store `Counter` in app-scoped state protected by a mutex or a serialized core actor. `get`, `increment`, `decrement`, and `reset` must be atomic with respect to concurrent invocations and must return the post-operation value. Destroying/reloading a webview must not destroy app state; destroying the app must release it.

Implement `hello-buttons` by composing the two fixtures' HTML and registering all five commands. Use the exact upstream IDs and command names (`form`, `name`, `message`, `counter`, `increment-btn`, `decrement-btn`, and `reset-btn`) so the same DOM-driving smoke harness can test both standalone and combined pages. The greeting response must remain independent of the counter value.

The native launch sequence is deliberately the same for both fixtures: load config and assets, create `main` window, create its `main` webview, inject the bridge before page code, wait for page-ready, then enter the adapter event loop. The C++ core owns command/state behavior; the selected adapter owns UI-thread dispatch, native handles, JavaScript evaluation, and webview teardown.

### Proposed `tests/` layout

The repository currently has no root `tests/` directory or test registrations. Add these later without compiling or invoking either vendored repository:

```text
tests/
        CMakeLists.txt
        hello_core.cpp
        buttons_core.cpp
        ipc_contract.cpp
        asset_protocol.cpp
        gui_smoke.cpp
        fixtures/
                hello/index.html
                hello_buttons/index.html
```

The frontend fixtures should initially be copies of the two upstream HTML files, with changes only when a deliberate compatibility gap is recorded. Keep the core tests independent of a display server and webview runtime.

### Proposed CTest cases

| Test | Scope | Required assertions |
| --- | --- | --- |
| `tauripp.hello.core` | `tauripp_core` only | Register `greet`; invoke with `Ada`; assert the exact string; assert missing `name` is a structured error. |
| `tauripp.buttons.core` | `tauripp_core` only | Assert `get == 0`; invoke `increment`, `increment`, `decrement`, `reset`; assert `1`, `2`, `1`, `0`; assert state is retained between requests. |
| `tauripp.hello-buttons.core` | `tauripp_core` only | Register `greet` and counter commands together; assert the greeting result is unchanged before and after counter operations. |
| `tauripp.ipc.hello` | Fake webview transport | Decode the Tauri-shaped envelope, dispatch `greet`, deliver one success callback, reject malformed/missing invoke keys, and never resolve twice. |
| `tauripp.ipc.buttons` | Fake webview transport | Dispatch payload-free calls in order and verify returned values and callback cleanup. Add a concurrent sequence to prove state serialization. |
| `tauripp.assets.hello` | Asset/protocol layer | Serve `index.html` with the expected MIME type and CSP; reject traversal/out-of-scope paths; preserve bytes needed by HTML, CSS, JS, and WASM assets. |
| `tauripp.gui.hello` | Native adapter plus webview | Create the 800x600 `main` window, load the document, expose `window.__TAURI__.core.invoke`, and verify title, heading, input, button, and message update. |
| `tauripp.gui.buttons` | Native adapter plus webview | Verify initial `0`, click increment/decrement/reset through JavaScript evaluation, verify each displayed result, reload, and verify app state remains unchanged. |
| `tauripp.gui.hello-buttons` | Native adapter plus webview | Verify the greeting form and counter controls in one document, submit `Ada`, perform the button sequence, and assert both DOM regions remain correct. |

The GUI tests should use the adapter's existing `eval_script_with_callback` equivalent to drive and inspect the DOM, so no Node, Rust, or external browser automation is needed. They must wait for page-ready and each invoke promise rather than sleep. For a headless or missing-runtime environment, configure the test as unavailable and skip it with an explicit reason; never turn an environment failure into a passing test.

When `BUILD_TESTING` is enabled, the future `tests/CMakeLists.txt` should build the four core/transport executables, call `add_test` for them, and add GUI tests only for the selected native adapter. Apply CTest labels such as `core`, `ipc`, `assets`, and `gui`; set a finite GUI timeout; and use a documented skip return code for platforms without the required display or webview runtime. The root CMake file should add `tests/` only under `BUILD_TESTING`.

The first implementation milestone is therefore not full Tauri parity. It is: unchanged hello HTML plus a composed hello-buttons fixture, identical command names and result shapes, identical global invoke call, app-scoped counter state, one local asset protocol, and deterministic CTest coverage for the core before any platform smoke test is enabled.

## Native webview choices

| OS/configuration | Webview | Main considerations |
| --- | --- | --- |
| Windows | WebView2 | COM, async environment/controller creation, installed Evergreen or packaged Fixed Version Runtime, postMessage, document-start scripts, custom resources. See [gui_win.md](gui_win.md). |
| Linux + GTK4 | WebKitGTK build matching GTK4, if available in the deployment target | The vendored Tauri code is GTK3/WebKitGTK 4.1. Do not reuse it as a GTK4 widget. Verify the actual WebKitGTK ABI and system package. |
| Linux + Qt6 | Qt WebEngine, or a deliberately bridged system webview | Qt WebEngine is Chromium-based and heavier, but has strong script, profile, cookie, request, and URL-scheme APIs. Keep it optional for performance. |
| macOS | WKWebView | System WebKit, `WKUserContentController` message handlers, navigation/UI delegates, custom schemes, data stores, and SwiftUI/AppKit hosting. See [gui_macos.md](gui_macos.md). |

Do not embed a second Chromium runtime in tauri++ merely to make Linux behavior uniform. Prefer the system webview and document feature differences.

## Performance-first dependencies

Start with the smallest native and portable set:

- `yyjson` or `simdjson` plus a compact writer for IPC; benchmark the chosen parser against the actual envelope. Avoid `nlohmann/json` in hot paths if allocation/latency matters.
- `std::filesystem` with canonicalization and an allowlist for assets; use a WHATWG URL parser such as `ada` only if URL behavior beyond the standard library is needed.
- a bounded MPSC queue first; use a specialized queue such as `moodycamel::ConcurrentQueue` only after profiling shows mutex contention.
- OS webview SDKs instead of shipping Chromium. Qt WebEngine is the explicit exception for the Qt configuration.
- defer updater cryptography, compression, HTTP, SQL, notifications, and shell/process helpers to optional plugins. Choose libraries such as libsodium, zstd, and standalone Asio only when the corresponding feature is required.

The performance-critical path should be: one parse, one permission lookup, one handler dispatch, and one response serialization. Use raw bytes for bulk transfers and never run blocking work on a GUI or webview callback thread.

## Implementation order

1. Build `tauripp_core` and `taopp` around the versioned C ABI; test command/event/resource/permission behavior without a GUI.
2. Add one native adapter per platform and a single Linux CMake choice (`GTK4` or `QT6`).
3. Add the WebView2, matching Linux webview, and WKWebView bridges.
4. Vendor/adapt the upstream API signatures and types so existing frontend imports work unchanged.
5. Add menus, tray, dialogs, filesystem, notifications, updater, bundling, and dynamic plugins as capability-gated modules.
6. Add golden IPC tests, ACL tests, lifecycle tests, binary-channel ordering tests, and one smoke application per native backend before claiming interchangeability.

## Local sources

- [Tauri architecture](../third_party/tauri/ARCHITECTURE.md)
- [Tauri API index](../third_party/tauri/packages/api/src/index.ts)
- [Tauri core API](../third_party/tauri/packages/api/src/core.ts)
- [Tauri events](../third_party/tauri/packages/api/src/event.ts)
- [Tauri window API](../third_party/tauri/packages/api/src/window.ts)
- [Tauri webview API](../third_party/tauri/packages/api/src/webview.ts)
- [Tauri runtime traits](../third_party/tauri/crates/tauri-runtime/src/lib.rs)
- [Tauri runtime webview traits](../third_party/tauri/crates/tauri-runtime/src/webview.rs)
- [Tauri WRY runtime](../third_party/tauri/crates/tauri-runtime-wry/src/lib.rs)
- [Tauri IPC protocol](../third_party/tauri/crates/tauri/src/ipc/protocol.rs)
- [Tauri channels](../third_party/tauri/crates/tauri/src/ipc/channel.rs)
- [Tauri plugin trait](../third_party/tauri/crates/tauri/src/plugin.rs)
- [Tauri asset protocol](../third_party/tauri/crates/tauri/src/protocol/asset.rs)
- [Tauri permissions](https://v2.tauri.app/security/permissions/)
- [Tauri API reference](https://v2.tauri.app/reference/javascript/api/)
