#include "taopp/native.hpp"

namespace taopp {

struct NativeApplication::Impl { };

NativeApplication::NativeApplication(EventLoop& event_loop):
    event_loop_(event_loop),
    impl_(std::make_unique<Impl>()) { }

NativeApplication::~NativeApplication() = default;

NativeResult NativeApplication::create_window(const WindowOptions&, WindowId&) {
    return NativeResult::unsupported("no native tao++ adapter is implemented for this operating system");
}

NativeResult NativeApplication::destroy_window(WindowId) {
    return NativeResult::unsupported("window destruction is unavailable without a native tao++ adapter");
}

NativeResult NativeApplication::add_control(WindowId, const ControlOptions&) {
    return NativeResult::unsupported("native controls are unavailable without a native tao++ adapter");
}

NativeResult NativeApplication::set_control_text(WindowId, ControlId, const std::string&) {
    return NativeResult::unsupported("native controls are unavailable without a native tao++ adapter");
}

std::optional<std::string> NativeApplication::control_text(WindowId, ControlId) const { return std::nullopt; }

std::optional<NativeWindowHandle> NativeApplication::native_window(WindowId) const { return std::nullopt; }

void NativeApplication::set_action_callback(ActionCallback) { }

void NativeApplication::set_close_requested_callback(CloseRequestedCallback) { }

int NativeApplication::run() { return static_cast<int>(NativeErrorCode::Unsupported); }

void NativeApplication::exit(int code) { event_loop_.exit(code); }

} // namespace taopp