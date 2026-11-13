#include "taopp/core.hpp"

#include <cassert>

int main() {
    taopp::EventLoop loop;
    const taopp::WindowId window = loop.create_window(taopp::WindowOptions {});
    assert(window != 0);
    assert(loop.window_state(window).has_value());

    int posted_value = 0;
    assert(loop.post([&posted_value] { posted_value = 42; }));
    assert(loop.run_iteration(std::chrono::milliseconds(0)));
    assert(posted_value == 42);

    taopp::Size resized { 1024, 768 };
    loop.emit_window_event({ taopp::WindowEventType::Resized, window, resized });
    const auto state = loop.window_state(window);
    assert(state.has_value());
    assert(state->physical_size.width == 1024);
    assert(state->physical_size.height == 768);

    assert(loop.destroy_window(window));
    assert(!loop.window_state(window).has_value());
    return 0;
}