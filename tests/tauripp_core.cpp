#include "tauripp/core.hpp"

#include <cassert>
#include <string>

namespace {

tauripp::InvokeResult invoke(tauripp::CommandRegistry& registry, std::string command, std::string payload = "null") {
    return registry.invoke({ std::move(command), std::move(payload) });
}

void run_hello() {
    tauripp::CommandRegistry registry;
    tauripp::CounterState counter;
    tauripp::register_hello_commands(registry, counter);

    const auto result = invoke(registry, "greet", R"({"name":"Ada"})");
    assert(result.success);
    assert(result.payload == R"("Hello Ada, You have been greeted from C++!")");

    const auto missing = invoke(registry, "greet", "{}");
    assert(!missing.success);
    assert(missing.error_payload.find("invalid-args") != std::string::npos);
}

void run_buttons() {
    tauripp::CommandRegistry registry;
    tauripp::CounterState counter;
    tauripp::register_hello_commands(registry, counter);

    assert(invoke(registry, "get").payload == "0");
    assert(invoke(registry, "increment").payload == "1");
    assert(invoke(registry, "increment").payload == "2");
    assert(invoke(registry, "decrement").payload == "1");
    assert(invoke(registry, "reset").payload == "0");
    assert(invoke(registry, "get").payload == "0");
}

void run_combined() {
    tauripp::CommandRegistry registry;
    tauripp::CounterState counter;
    tauripp::register_hello_commands(registry, counter);

    assert(invoke(registry, "increment").payload == "1");
    assert(invoke(registry, "greet", R"({"name":"Ada"})").payload == R"("Hello Ada, You have been greeted from C++!")");
    assert(invoke(registry, "get").payload == "1");
}

} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string mode(argv[1]);
    if (mode == "hello") {
        run_hello();
    } else if (mode == "buttons") {
        run_buttons();
    } else if (mode == "hello-buttons") {
        run_combined();
    } else {
        return 2;
    }
    return 0;
}