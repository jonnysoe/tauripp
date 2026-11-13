#pragma once

#include "taopp/core.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace taopp {

enum class NativeErrorCode : std::int32_t {
    Ok = 0,
    InvalidArgument = 1,
    Unsupported = 2,
    SystemError = 3,
};

struct NativeResult {
    NativeErrorCode code = NativeErrorCode::Ok;
    std::string message;

    explicit operator bool() const { return code == NativeErrorCode::Ok; }

    static NativeResult success() { return {}; }

    static NativeResult invalid(std::string message) {
        return { NativeErrorCode::InvalidArgument, std::move(message) };
    }

    static NativeResult unsupported(std::string message) {
        return { NativeErrorCode::Unsupported, std::move(message) };
    }

    static NativeResult system_error(std::string message) {
        return { NativeErrorCode::SystemError, std::move(message) };
    }
};

enum class NativeWindowHandleKind : std::uint32_t {
    None = 0,
    Win32 = 1,
    Gtk = 2,
    Qt = 3,
    AppKit = 4,
};

struct NativeWindowHandle {
    NativeWindowHandleKind kind = NativeWindowHandleKind::None;
    std::uintptr_t value = 0;
};

using ActionCallback = std::function<void(WindowId, ControlId, std::string)>;
using CloseRequestedCallback = std::function<bool(WindowId)>;

class NativeApplication {
public:
    explicit NativeApplication(EventLoop& event_loop);
    ~NativeApplication();

    NativeApplication(const NativeApplication&) = delete;
    NativeApplication& operator=(const NativeApplication&) = delete;

    NativeResult create_window(const WindowOptions& options, WindowId& id);
    NativeResult destroy_window(WindowId id);
    NativeResult add_control(WindowId window, const ControlOptions& options);
    NativeResult set_control_text(WindowId window, ControlId control, const std::string& text);
    std::optional<std::string> control_text(WindowId window, ControlId control) const;
    std::optional<NativeWindowHandle> native_window(WindowId window) const;

    void set_action_callback(ActionCallback callback);
    void set_close_requested_callback(CloseRequestedCallback callback);

    int run();
    void exit(int code = 0);

private:
    struct Impl;

    EventLoop& event_loop_;
    std::unique_ptr<Impl> impl_;
};

} // namespace taopp