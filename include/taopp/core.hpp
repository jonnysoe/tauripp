#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace taopp {

using WindowId = std::uint64_t;
using ControlId = std::uint64_t;
using CallbackId = std::uint64_t;

struct Size {
    std::int32_t width = 0;
    std::int32_t height = 0;
};

struct Position {
    std::int32_t x = 0;
    std::int32_t y = 0;
};

struct Rect {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

struct WindowOptions {
    std::string title = "taopp";
    Size logical_size { 800, 600 };
    Position logical_position {};
    bool has_position = false;
    bool visible = true;
    bool resizable = true;
    bool decorated = true;
    bool fullscreen = false;
    bool focused = true;
};

struct WindowState {
    WindowId id = 0;
    WindowOptions options;
    Size physical_size { 800, 600 };
    Position physical_position {};
    float scale_factor = 1.0F;
    bool visible = false;
    bool focused = false;
    bool fullscreen = false;
    bool maximized = false;
    bool minimized = false;
};

enum class ControlType {
    Label,
    TextInput,
    Button,
    Toggle,
};

struct ControlOptions {
    ControlId id = 0;
    ControlType type = ControlType::Label;
    std::string text;
    Rect logical_bounds {};
    bool enabled = true;
    double font_size = 15.0;
    bool emphasized = false;
    bool accent = false;
    bool checked = false;
};

enum class WindowEventType {
    Created,
    Resized,
    Moved,
    CloseRequested,
    Destroyed,
    Focused,
    Unfocused,
    ScaleFactorChanged,
    RedrawRequested,
};

struct WindowEvent {
    WindowEventType type = WindowEventType::Created;
    WindowId window = 0;
    Size size {};
    Position position {};
    float scale_factor = 1.0F;
};

enum class EventType {
    NewEvents,
    Window,
    User,
    MainEventsCleared,
    RedrawRequested,
    RedrawEventsCleared,
    LoopDestroyed,
};

struct Event {
    EventType type = EventType::NewEvents;
    WindowEvent window;
    std::uint64_t user_id = 0;
    std::string user_payload;
};

enum class RunMode {
    Poll,
    Wait,
    Exit,
};

namespace detail {
    struct EventLoopState;
}

class EventLoopProxy {
public:
    EventLoopProxy() = default;

    bool post(std::function<void()> task) const;
    void wake() const;

private:
    explicit EventLoopProxy(std::weak_ptr<detail::EventLoopState> state);

    std::weak_ptr<detail::EventLoopState> state_;
    friend class EventLoop;
};

class EventLoop {
public:
    using EventCallback = std::function<void(const Event&)>;
    using WakeCallback = std::function<void()>;

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    WindowId create_window(const WindowOptions& options);
    bool destroy_window(WindowId id);
    std::optional<WindowState> window_state(WindowId id) const;

    CallbackId subscribe(EventCallback callback);
    bool unsubscribe(CallbackId id);

    bool post(std::function<void()> task);
    EventLoopProxy proxy() const;
    void set_wake_callback(WakeCallback callback);

    void emit_window_event(const WindowEvent& event);
    void emit_user_event(std::uint64_t id, std::string payload);
    void request_redraw(WindowId id);

    bool process_pending();
    bool run_iteration(std::chrono::milliseconds wait_for);
    int run(RunMode mode = RunMode::Wait);
    void exit(int code = 0);
    bool exit_requested() const;
    int exit_code() const;

private:
    std::shared_ptr<detail::EventLoopState> state_;
};

} // namespace taopp