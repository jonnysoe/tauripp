#pragma once

#include "taopp/native.hpp"
#include "tauripp/core.hpp"

#include <memory>
#include <optional>
#include <string>

namespace tauripp {

class Application {
public:
    explicit Application(std::string runtime_key = "tauripp-runtime-key");
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    CommandRegistry& commands();
    IpcRuntime& ipc();
    InvokeResult invoke(std::string command, std::string payload = "null");

    taopp::NativeResult create_window(const taopp::WindowOptions& options, taopp::WindowId& id);
    taopp::NativeResult destroy_window(taopp::WindowId id);
    taopp::NativeResult add_control(taopp::WindowId window, const taopp::ControlOptions& options);
    taopp::NativeResult set_control_text(taopp::WindowId window, taopp::ControlId control, const std::string& text);
    std::optional<std::string> control_text(taopp::WindowId window, taopp::ControlId control) const;
    std::optional<taopp::NativeWindowHandle> native_window(taopp::WindowId window) const;

    void set_action_callback(taopp::ActionCallback callback);
    void set_close_requested_callback(taopp::CloseRequestedCallback callback);

    int run();
    void exit(int code = 0);
    const std::string& last_error() const;

private:
    class Impl;

    std::unique_ptr<Impl> impl_;
    std::string last_error_;
};

const char* version() noexcept;

} // namespace tauripp