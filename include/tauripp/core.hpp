#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace tauripp {

struct InvokeRequest {
    std::string command;
    std::string payload;
    std::string webview_label = "main";
    std::string origin = "tauripp://localhost";
};

struct InvokeResult {
    bool success = false;
    std::string payload;
    std::string error_payload;

    static InvokeResult ok(std::string payload) { return { true, std::move(payload), {} }; }

    static InvokeResult error(std::string payload) { return { false, {}, std::move(payload) }; }
};

class CommandRegistry {
public:
    using Handler = std::function<InvokeResult(const InvokeRequest&)>;

    bool register_command(std::string name, Handler handler);
    bool unregister_command(std::string_view name);
    bool contains(std::string_view name) const;
    InvokeResult invoke(const InvokeRequest& request) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Handler> handlers_;
};

class CounterState {
public:
    std::int64_t get() const;
    std::int64_t increment();
    std::int64_t decrement();
    std::int64_t reset();

private:
    mutable std::mutex mutex_;
    std::int64_t value_ = 0;
};

void register_hello_commands(CommandRegistry& registry, CounterState& counter);

std::string json_quote(std::string_view value);
bool json_decode_string(std::string_view value, std::string& decoded);
bool json_string_field(std::string_view object, std::string_view field, std::string& value, std::string& error);
bool json_has_no_arguments(std::string_view payload);

struct IpcResponse {
    bool accepted = false;
    bool success = false;
    std::uint64_t callback_id = 0;
    std::uint64_t error_callback_id = 0;
    std::string payload;
    std::string error_payload;
};

class IpcRuntime {
public:
    using Callback = std::function<void(const IpcResponse&)>;

    IpcRuntime(CommandRegistry& registry, std::string runtime_key);

    bool register_callback(std::uint64_t id, Callback callback);
    bool unregister_callback(std::uint64_t id);
    std::size_t pending_callback_count() const;
    IpcResponse dispatch(std::string_view envelope);

private:
    CommandRegistry& registry_;
    std::string runtime_key_;
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, Callback> callbacks_;
};

} // namespace tauripp