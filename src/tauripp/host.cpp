#include "tauripp/application.hpp"

#include <utility>

namespace tauripp {

class Application::Impl {
public:
    explicit Impl(std::string runtime_key):
        event_loop(),
        native(event_loop),
        commands(),
        ipc(commands, std::move(runtime_key)) { }

    taopp::EventLoop event_loop;
    taopp::NativeApplication native;
    CommandRegistry commands;
    IpcRuntime ipc;
};

Application::Application(std::string runtime_key): impl_(std::make_unique<Impl>(std::move(runtime_key))) { }

Application::~Application() = default;

CommandRegistry& Application::commands() { return impl_->commands; }

IpcRuntime& Application::ipc() { return impl_->ipc; }

InvokeResult Application::invoke(std::string command, std::string payload) {
    InvokeRequest request;
    request.command = std::move(command);
    request.payload = std::move(payload);
    request.webview_label = "main";
    request.origin = "tauripp://localhost";
    return impl_->commands.invoke(request);
}

taopp::NativeResult Application::create_window(const taopp::WindowOptions& options, taopp::WindowId& id) {
    const taopp::NativeResult result = impl_->native.create_window(options, id);
    last_error_ = result ? std::string {} : result.message;
    return result;
}

taopp::NativeResult Application::destroy_window(taopp::WindowId id) {
    const taopp::NativeResult result = impl_->native.destroy_window(id);
    last_error_ = result ? std::string {} : result.message;
    return result;
}

taopp::NativeResult Application::add_control(taopp::WindowId window, const taopp::ControlOptions& options) {
    const taopp::NativeResult result = impl_->native.add_control(window, options);
    last_error_ = result ? std::string {} : result.message;
    return result;
}

taopp::NativeResult
Application::set_control_text(taopp::WindowId window, taopp::ControlId control, const std::string& text) {
    const taopp::NativeResult result = impl_->native.set_control_text(window, control, text);
    last_error_ = result ? std::string {} : result.message;
    return result;
}

std::optional<std::string> Application::control_text(taopp::WindowId window, taopp::ControlId control) const {
    return impl_->native.control_text(window, control);
}

std::optional<taopp::NativeWindowHandle> Application::native_window(taopp::WindowId window) const {
    return impl_->native.native_window(window);
}

void Application::set_action_callback(taopp::ActionCallback callback) {
    impl_->native.set_action_callback(std::move(callback));
}

void Application::set_close_requested_callback(taopp::CloseRequestedCallback callback) {
    impl_->native.set_close_requested_callback(std::move(callback));
}

int Application::run() { return impl_->native.run(); }

void Application::exit(int code) { impl_->native.exit(code); }

const std::string& Application::last_error() const { return last_error_; }

} // namespace tauripp