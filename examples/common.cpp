#include "common.hpp"

#include "tauripp/application.hpp"

#include <iostream>
#include <string>
#include <utility>

namespace tauripp_examples {

namespace {

    constexpr taopp::ControlId kHeading = 1;
    constexpr taopp::ControlId kNameInput = 2;
    constexpr taopp::ControlId kGreetButton = 3;
    constexpr taopp::ControlId kMessage = 4;
    constexpr taopp::ControlId kCounterLabel = 5;
    constexpr taopp::ControlId kCounterValue = 6;
    constexpr taopp::ControlId kIncrementButton = 7;
    constexpr taopp::ControlId kDecrementButton = 8;
    constexpr taopp::ControlId kResetButton = 9;
    constexpr taopp::ControlId kCounterToggle = 10;

    taopp::ControlOptions label(taopp::ControlId id, std::string text, taopp::Rect bounds) {
        return { id, taopp::ControlType::Label, std::move(text), bounds, true };
    }

    taopp::ControlOptions input(taopp::ControlId id, taopp::Rect bounds) {
        return { id, taopp::ControlType::TextInput, {}, bounds, true };
    }

    taopp::ControlOptions button(taopp::ControlId id, std::string text, taopp::Rect bounds) {
        return { id, taopp::ControlType::Button, std::move(text), bounds, true };
    }

    taopp::ControlOptions primary_button(taopp::ControlId id, std::string text, taopp::Rect bounds) {
        auto result = button(id, std::move(text), bounds);
        result.font_size = 16.0;
        result.emphasized = true;
        result.accent = true;
        return result;
    }

    taopp::ControlOptions toggle(taopp::ControlId id, std::string text, taopp::Rect bounds) {
        return { id, taopp::ControlType::Toggle, std::move(text), bounds, true };
    }

    bool add_hello_controls(tauripp::Application& app, taopp::WindowId window) {
        auto heading = label(kHeading, "Tauri++ hello world", { 50, 55, 650, 34 });
        heading.font_size = 24.0;
        heading.emphasized = true;
        auto name_input = input(kNameInput, { 50, 108, 360, 38 });
        name_input.font_size = 16.0;
        auto greet_button = primary_button(kGreetButton, "Greet", { 430, 108, 140, 38 });
        auto message = label(kMessage, {}, { 50, 165, 650, 30 });
        message.font_size = 16.0;
        return app.add_control(window, heading) &&
               app.add_control(window, name_input) &&
               app.add_control(window, greet_button) &&
               app.add_control(window, message);
    }

    bool add_counter_controls(tauripp::Application& app, taopp::WindowId window, bool below_hello) {
        const int top = below_hello ? 220 : 70;
        auto heading = label(kCounterLabel, "Counter", { 50, top, 650, 30 });
        heading.font_size = 18.0;
        heading.emphasized = true;
        auto value = label(kCounterValue, "0", { 50, top + 42, 70, 38 });
        value.font_size = 26.0;
        value.emphasized = true;
        auto increment = primary_button(kIncrementButton, "Increment", { 140, top + 42, 112, 36 });
        auto decrement = button(kDecrementButton, "Decrement", { 264, top + 42, 112, 36 });
        decrement.font_size = 16.0;
        auto reset = button(kResetButton, "Reset", { 388, top + 42, 90, 36 });
        reset.font_size = 16.0;
        auto state_toggle = toggle(kCounterToggle, "Counter enabled", { 50, top + 100, 220, 64 });
        state_toggle.font_size = 16.0;
        return app.add_control(window, heading) &&
               app.add_control(window, value) &&
               app.add_control(window, increment) &&
               app.add_control(window, decrement) &&
               app.add_control(window, reset) &&
               app.add_control(window, state_toggle);
    }

    void set_counter_value(tauripp::Application& app, taopp::WindowId window, const tauripp::InvokeResult& result) {
        app.set_control_text(window, kCounterValue, result.success ? result.payload : "error");
    }

} // namespace

int run_fixture(bool include_hello, bool include_buttons) {
    tauripp::Application app;
    tauripp::CounterState counter;
    tauripp::register_hello_commands(app.commands(), counter);

    taopp::WindowOptions options;
    options.title = "Welcome to Tauri!";
    options.logical_size = { 800, 600 };
    options.visible = true;
    options.resizable = true;
    options.decorated = true;
    options.fullscreen = false;
    options.focused = true;

    taopp::WindowId window = 0;
    if (!app.create_window(options, window)) {
        std::cerr << app.last_error() << '\n';
        return 1;
    }
    if ((include_hello && !add_hello_controls(app, window)) ||
        (include_buttons && !add_counter_controls(app, window, include_hello))) {
        std::cerr << app.last_error() << '\n';
        return 1;
    }

    if (include_buttons) {
        set_counter_value(app, window, app.invoke("get"));
    }

    app.set_action_callback([&app, window](taopp::WindowId action_window, taopp::ControlId control, std::string text) {
        if (action_window != window) {
            return;
        }
        if (control == kGreetButton) {
            const auto name = app.control_text(window, kNameInput);
            const std::string encoded_name = name.has_value() ? *name : std::string {};
            const tauripp::InvokeResult result =
                app.invoke("greet", "{\"name\":" + tauripp::json_quote(encoded_name) + "}");
            if (!result.success) {
                app.set_control_text(window, kMessage, result.error_payload);
                return;
            }
            std::string message;
            if (tauripp::json_decode_string(result.payload, message)) {
                app.set_control_text(window, kMessage, message);
            }
        } else if (control == kIncrementButton) {
            set_counter_value(app, window, app.invoke("increment"));
        } else if (control == kDecrementButton) {
            set_counter_value(app, window, app.invoke("decrement"));
        } else if (control == kResetButton) {
            set_counter_value(app, window, app.invoke("reset"));
        } else if (control == kCounterToggle) {
            app.set_control_text(window, kCounterLabel, text == "on" ? "Counter  |  enabled" : "Counter  |  paused");
        }
    });
    app.set_close_requested_callback([](taopp::WindowId) { return true; });

    return app.run();
}

} // namespace tauripp_examples