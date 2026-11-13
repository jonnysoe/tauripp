#include "taopp/core.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace taopp::detail {

struct EventLoopState {
    mutable std::mutex mutex;
    std::condition_variable condition;
    std::deque<std::function<void()>> tasks;
    std::unordered_map<WindowId, WindowState> windows;
    std::unordered_map<CallbackId, EventLoop::EventCallback> callbacks;
    EventLoop::WakeCallback wake_callback;
    WindowId next_window_id = 1;
    CallbackId next_callback_id = 1;
    bool exit_requested = false;
    int exit_code = 0;
};

} // namespace taopp::detail

namespace taopp {

namespace {

    void dispatch_event(const std::shared_ptr<detail::EventLoopState>& state, const Event& event) {
        std::vector<EventLoop::EventCallback> callbacks;
        {
            std::lock_guard lock(state->mutex);
            callbacks.reserve(state->callbacks.size());
            for (const auto& entry : state->callbacks) {
                callbacks.push_back(entry.second);
            }
        }

        for (const auto& callback : callbacks) {
            try {
                callback(event);
            } catch (...) {
                // Event subscribers cannot be allowed to break the native loop.
            }
        }
    }

    void notify(const std::shared_ptr<detail::EventLoopState>& state) {
        EventLoop::WakeCallback wake_callback;
        {
            std::lock_guard lock(state->mutex);
            wake_callback = state->wake_callback;
        }
        state->condition.notify_one();
        if (wake_callback) {
            try {
                wake_callback();
            } catch (...) {
                // A wake hook is advisory; the condition variable remains usable.
            }
        }
    }

} // namespace

EventLoopProxy::EventLoopProxy(std::weak_ptr<detail::EventLoopState> state): state_(std::move(state)) { }

bool EventLoopProxy::post(std::function<void()> task) const {
    auto state = state_.lock();
    if (!state || !task) {
        return false;
    }

    {
        std::lock_guard lock(state->mutex);
        if (state->exit_requested) {
            return false;
        }
        state->tasks.push_back(std::move(task));
    }
    notify(state);
    return true;
}

void EventLoopProxy::wake() const {
    if (auto state = state_.lock()) {
        notify(state);
    }
}

EventLoop::EventLoop(): state_(std::make_shared<detail::EventLoopState>()) { }

EventLoop::~EventLoop() {
    exit();
    std::lock_guard lock(state_->mutex);
    state_->callbacks.clear();
    state_->tasks.clear();
    state_->windows.clear();
}

WindowId EventLoop::create_window(const WindowOptions& options) {
    WindowState state;
    {
        std::lock_guard lock(state_->mutex);
        state.id = state_->next_window_id++;
        state.options = options;
        state.physical_size = options.logical_size;
        state.physical_position = options.logical_position;
        state.visible = options.visible;
        state.focused = options.focused;
        state.fullscreen = options.fullscreen;
        state_->windows.emplace(state.id, state);
    }

    emit_window_event(
        WindowEvent { WindowEventType::Created, state.id, state.physical_size, state.physical_position, 1.0F }
    );
    return state.id;
}

bool EventLoop::destroy_window(WindowId id) {
    {
        std::lock_guard lock(state_->mutex);
        if (state_->windows.erase(id) == 0) {
            return false;
        }
    }

    emit_window_event(WindowEvent { WindowEventType::Destroyed, id });
    return true;
}

std::optional<WindowState> EventLoop::window_state(WindowId id) const {
    std::lock_guard lock(state_->mutex);
    const auto found = state_->windows.find(id);
    if (found == state_->windows.end()) {
        return std::nullopt;
    }
    return found->second;
}

CallbackId EventLoop::subscribe(EventCallback callback) {
    if (!callback) {
        return 0;
    }

    std::lock_guard lock(state_->mutex);
    const CallbackId id = state_->next_callback_id++;
    state_->callbacks.emplace(id, std::move(callback));
    return id;
}

bool EventLoop::unsubscribe(CallbackId id) {
    std::lock_guard lock(state_->mutex);
    return state_->callbacks.erase(id) != 0;
}

bool EventLoop::post(std::function<void()> task) { return proxy().post(std::move(task)); }

EventLoopProxy EventLoop::proxy() const { return EventLoopProxy(state_); }

void EventLoop::set_wake_callback(WakeCallback callback) {
    std::lock_guard lock(state_->mutex);
    state_->wake_callback = std::move(callback);
}

void EventLoop::emit_window_event(const WindowEvent& event) {
    {
        std::lock_guard lock(state_->mutex);
        const auto found = state_->windows.find(event.window);
        if (found != state_->windows.end()) {
            auto& window = found->second;
            switch (event.type) {
            case WindowEventType::Resized:
                window.physical_size = event.size;
                break;
            case WindowEventType::Moved:
                window.physical_position = event.position;
                break;
            case WindowEventType::Focused:
                window.focused = true;
                break;
            case WindowEventType::Unfocused:
                window.focused = false;
                break;
            case WindowEventType::ScaleFactorChanged:
                window.scale_factor = event.scale_factor;
                break;
            case WindowEventType::Destroyed:
                state_->windows.erase(found);
                break;
            default:
                break;
            }
        }
    }

    Event wrapper;
    wrapper.type = event.type == WindowEventType::RedrawRequested ? EventType::RedrawRequested : EventType::Window;
    wrapper.window = event;
    dispatch_event(state_, wrapper);
}

void EventLoop::emit_user_event(std::uint64_t id, std::string payload) {
    Event event;
    event.type = EventType::User;
    event.user_id = id;
    event.user_payload = std::move(payload);
    dispatch_event(state_, event);
}

void EventLoop::request_redraw(WindowId id) { emit_window_event(WindowEvent { WindowEventType::RedrawRequested, id }); }

bool EventLoop::process_pending() {
    std::deque<std::function<void()>> tasks;
    {
        std::lock_guard lock(state_->mutex);
        if (state_->exit_requested) {
            return false;
        }
        tasks.swap(state_->tasks);
    }

    dispatch_event(state_, Event { EventType::NewEvents });
    for (auto& task : tasks) {
        try {
            task();
        } catch (...) {
            // Posted work is isolated from the loop and from other tasks.
        }
    }
    dispatch_event(state_, Event { EventType::MainEventsCleared });
    return !exit_requested();
}

bool EventLoop::run_iteration(std::chrono::milliseconds wait_for) {
    if (wait_for.count() > 0) {
        std::unique_lock lock(state_->mutex);
        state_->condition.wait_for(lock, wait_for, [this] { return state_->exit_requested || !state_->tasks.empty(); });
    }
    return process_pending();
}

int EventLoop::run(RunMode mode) {
    if (mode == RunMode::Exit) {
        exit();
        return exit_code();
    }

    if (mode == RunMode::Poll) {
        process_pending();
    } else {
        while (run_iteration(std::chrono::milliseconds::max())) { }
    }

    const int code = exit_code();
    dispatch_event(state_, Event { EventType::LoopDestroyed });
    return code;
}

void EventLoop::exit(int code) {
    {
        std::lock_guard lock(state_->mutex);
        state_->exit_requested = true;
        state_->exit_code = code;
    }
    notify(state_);
}

bool EventLoop::exit_requested() const {
    std::lock_guard lock(state_->mutex);
    return state_->exit_requested;
}

int EventLoop::exit_code() const {
    std::lock_guard lock(state_->mutex);
    return state_->exit_code;
}

} // namespace taopp