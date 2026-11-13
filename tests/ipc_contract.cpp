#include "tauripp/core.hpp"

#include <cassert>

int main() {
    tauripp::CommandRegistry registry;
    tauripp::CounterState counter;
    tauripp::register_hello_commands(registry, counter);
    tauripp::IpcRuntime runtime(registry, "test-runtime-key");

    int success_callbacks = 0;
    int error_callbacks = 0;
    assert(runtime.register_callback(12, [&success_callbacks](const tauripp::IpcResponse& response) {
        ++success_callbacks;
        assert(response.success);
        assert(response.payload == R"("Hello Ada, You have been greeted from C++!")");
    }));
    assert(runtime.register_callback(13, [&error_callbacks](const tauripp::IpcResponse& response) {
        ++error_callbacks;
        assert(!response.success);
    }));

    const auto success = runtime.dispatch(
        R"({"cmd":"greet","callback":12,"error":13,"payload":{"name":"Ada"},"__TAURI_INVOKE_KEY__":"test-runtime-key"})"
    );
    assert(success.accepted);
    assert(success.success);
    assert(success_callbacks == 1);
    assert(error_callbacks == 0);
    assert(runtime.pending_callback_count() == 0);

    assert(runtime.register_callback(14, [](const tauripp::IpcResponse&) { }));
    assert(runtime.register_callback(15, [&error_callbacks](const tauripp::IpcResponse& response) {
        ++error_callbacks;
        assert(!response.success);
        assert(response.error_payload.find("invalid-invoke-key") != std::string::npos);
    }));
    const auto rejected = runtime.dispatch(
        R"({"cmd":"greet","callback":14,"error":15,"payload":{"name":"Ada"},"__TAURI_INVOKE_KEY__":"wrong"})"
    );
    assert(!rejected.accepted);
    assert(!rejected.success);
    assert(error_callbacks == 1);
    assert(runtime.pending_callback_count() == 0);

    const auto malformed = runtime.dispatch("not-json");
    assert(!malformed.accepted);
    assert(malformed.error_payload.find("invalid-envelope") != std::string::npos);
    return 0;
}