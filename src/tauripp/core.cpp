#include "tauripp/core.hpp"

#include <charconv>
#include <cstddef>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

namespace tauripp {

namespace {

    std::string error_object(std::string_view code, std::string_view message) {
        return "{\"code\":" + json_quote(code) + ",\"message\":" + json_quote(message) + "}";
    }

    class JsonParser {
    public:
        explicit JsonParser(std::string_view input): input_(input) { }

        bool parse_object(std::unordered_map<std::string, std::string>& fields, std::string& error) {
            skip_space();
            if (!consume('{')) {
                error = "JSON value must be an object";
                return false;
            }

            skip_space();
            if (consume('}')) {
                skip_space();
                if (!at_end()) {
                    error = "trailing data after JSON object";
                    return false;
                }
                return true;
            }

            while (true) {
                std::string key;
                if (!parse_string(key)) {
                    error = "object keys must be JSON strings";
                    return false;
                }
                skip_space();
                if (!consume(':')) {
                    error = "missing ':' after JSON object key";
                    return false;
                }

                std::string raw_value;
                if (!parse_value(raw_value)) {
                    error = "invalid JSON object value";
                    return false;
                }
                fields[std::move(key)] = std::move(raw_value);

                skip_space();
                if (consume('}')) {
                    skip_space();
                    if (!at_end()) {
                        error = "trailing data after JSON object";
                        return false;
                    }
                    return true;
                }
                if (!consume(',')) {
                    error = "missing ',' between JSON object fields";
                    return false;
                }
                skip_space();
            }
        }

        bool parse_string_value(std::string& value, std::string& error) {
            skip_space();
            if (!parse_string(value)) {
                error = "JSON value must be a string";
                return false;
            }
            skip_space();
            if (!at_end()) {
                error = "trailing data after JSON string";
                return false;
            }
            return true;
        }

    private:
        static bool is_hex(char character) {
            return (character >= '0' && character <= '9') ||
                   (character >= 'a' && character <= 'f') ||
                   (character >= 'A' && character <= 'F');
        }

        static std::uint32_t hex_value(char character) {
            if (character >= '0' && character <= '9') {
                return static_cast<std::uint32_t>(character - '0');
            }
            if (character >= 'a' && character <= 'f') {
                return static_cast<std::uint32_t>(character - 'a' + 10);
            }
            return static_cast<std::uint32_t>(character - 'A' + 10);
        }

        void append_code_point(std::uint32_t code_point, std::string& output) {
            if (code_point <= 0x7F) {
                output.push_back(static_cast<char>(code_point));
            } else if (code_point <= 0x7FF) {
                output.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
                output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
            } else if (code_point <= 0xFFFF) {
                output.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
                output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
            } else if (code_point <= 0x10FFFF) {
                output.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
                output.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
            }
        }

        bool parse_string(std::string& output) {
            if (!consume('"')) {
                return false;
            }

            output.clear();
            while (!at_end()) {
                const char character = input_[cursor_++];
                if (character == '"') {
                    return true;
                }
                if (static_cast<unsigned char>(character) < 0x20) {
                    return false;
                }
                if (character != '\\') {
                    output.push_back(character);
                    continue;
                }
                if (at_end()) {
                    return false;
                }

                const char escape = input_[cursor_++];
                switch (escape) {
                case '"':
                case '\\':
                case '/':
                    output.push_back(escape);
                    break;
                case 'b':
                    output.push_back('\b');
                    break;
                case 'f':
                    output.push_back('\f');
                    break;
                case 'n':
                    output.push_back('\n');
                    break;
                case 'r':
                    output.push_back('\r');
                    break;
                case 't':
                    output.push_back('\t');
                    break;
                case 'u': {
                    if (cursor_ + 4 > input_.size() ||
                        !is_hex(input_[cursor_]) ||
                        !is_hex(input_[cursor_ + 1]) ||
                        !is_hex(input_[cursor_ + 2]) ||
                        !is_hex(input_[cursor_ + 3])) {
                        return false;
                    }
                    std::uint32_t code_point = 0;
                    for (std::size_t index = 0; index < 4; ++index) {
                        code_point = (code_point << 4) | hex_value(input_[cursor_++]);
                    }
                    append_code_point(code_point, output);
                    break;
                }
                default:
                    return false;
                }
            }
            return false;
        }

        bool parse_value(std::string& raw_value) {
            skip_space();
            const std::size_t start = cursor_;
            if (at_end()) {
                return false;
            }

            if (input_[cursor_] == '"') {
                std::string ignored;
                if (!parse_string(ignored)) {
                    return false;
                }
            } else if (input_[cursor_] == '{') {
                ++cursor_;
                skip_space();
                if (!consume('}')) {
                    while (true) {
                        std::string ignored_key;
                        if (!parse_string(ignored_key)) {
                            return false;
                        }
                        skip_space();
                        if (!consume(':') || !parse_value(raw_value)) {
                            return false;
                        }
                        skip_space();
                        if (consume('}')) {
                            break;
                        }
                        if (!consume(',')) {
                            return false;
                        }
                        skip_space();
                    }
                }
            } else if (input_[cursor_] == '[') {
                ++cursor_;
                skip_space();
                if (!consume(']')) {
                    while (true) {
                        if (!parse_value(raw_value)) {
                            return false;
                        }
                        skip_space();
                        if (consume(']')) {
                            break;
                        }
                        if (!consume(',')) {
                            return false;
                        }
                        skip_space();
                    }
                }
            } else if (input_.substr(cursor_, 4) == "true") {
                cursor_ += 4;
            } else if (input_.substr(cursor_, 5) == "false") {
                cursor_ += 5;
            } else if (input_.substr(cursor_, 4) == "null") {
                cursor_ += 4;
            } else if (!parse_number()) {
                return false;
            }

            raw_value = std::string(input_.substr(start, cursor_ - start));
            return true;
        }

        bool parse_number() {
            const std::size_t start = cursor_;
            consume('-');
            if (consume('0')) {
                // A zero cannot be followed by another integer digit.
                if (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') {
                    cursor_ = start;
                    return false;
                }
            } else {
                if (cursor_ >= input_.size() || input_[cursor_] < '1' || input_[cursor_] > '9') {
                    cursor_ = start;
                    return false;
                }
                while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') {
                    ++cursor_;
                }
            }
            if (consume('.')) {
                const std::size_t fraction_start = cursor_;
                while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') {
                    ++cursor_;
                }
                if (fraction_start == cursor_) {
                    cursor_ = start;
                    return false;
                }
            }
            if (cursor_ < input_.size() && (input_[cursor_] == 'e' || input_[cursor_] == 'E')) {
                ++cursor_;
                consume('+') || consume('-');
                const std::size_t exponent_start = cursor_;
                while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') {
                    ++cursor_;
                }
                if (exponent_start == cursor_) {
                    cursor_ = start;
                    return false;
                }
            }
            return cursor_ != start;
        }

        void skip_space() {
            while (cursor_ < input_.size()) {
                const char character = input_[cursor_];
                if (character != ' ' && character != '\t' && character != '\r' && character != '\n') {
                    break;
                }
                ++cursor_;
            }
        }

        bool consume(char expected) {
            if (cursor_ < input_.size() && input_[cursor_] == expected) {
                ++cursor_;
                return true;
            }
            return false;
        }

        bool at_end() const { return cursor_ == input_.size(); }

        std::string_view input_;
        std::size_t cursor_ = 0;
    };

    bool parse_json_string(std::string_view value, std::string& decoded) {
        std::string error;
        JsonParser parser(value);
        return parser.parse_string_value(decoded, error);
    }

    bool parse_callback_id(std::string_view raw_value, std::uint64_t& id) {
        if (raw_value.empty() || raw_value.front() == '-' || raw_value.find_first_of(".eE") != std::string_view::npos) {
            return false;
        }
        const auto result = std::from_chars(raw_value.data(), raw_value.data() + raw_value.size(), id);
        return result.ec == std::errc {} && result.ptr == raw_value.data() + raw_value.size();
    }

    std::string raw_field(const std::unordered_map<std::string, std::string>& fields, std::string_view name) {
        const auto found = fields.find(std::string(name));
        return found == fields.end() ? std::string {} : found->second;
    }

} // namespace

std::string json_quote(std::string_view value) {
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('"');
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (character < 0x20) {
                std::ostringstream escaped;
                escaped << "\\u" << std::hex << static_cast<int>(character);
                result += escaped.str();
            } else {
                result.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    result.push_back('"');
    return result;
}

bool json_decode_string(std::string_view value, std::string& decoded) { return parse_json_string(value, decoded); }

bool json_string_field(std::string_view object, std::string_view field, std::string& value, std::string& error) {
    std::unordered_map<std::string, std::string> fields;
    JsonParser parser(object);
    if (!parser.parse_object(fields, error)) {
        return false;
    }
    const std::string raw_value = raw_field(fields, field);
    if (raw_value.empty()) {
        error = "missing required field '" + std::string(field) + "'";
        return false;
    }
    if (!parse_json_string(raw_value, value)) {
        error = "field '" + std::string(field) + "' must be a string";
        return false;
    }
    return true;
}

bool json_has_no_arguments(std::string_view payload) {
    std::string trimmed(payload);
    const auto first = trimmed.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return true;
    }
    const auto last = trimmed.find_last_not_of(" \t\r\n");
    trimmed = trimmed.substr(first, last - first + 1);
    return trimmed == "null" || trimmed == "{}";
}

bool CommandRegistry::register_command(std::string name, Handler handler) {
    if (name.empty() || !handler) {
        return false;
    }
    std::lock_guard lock(mutex_);
    return handlers_.emplace(std::move(name), std::move(handler)).second;
}

bool CommandRegistry::unregister_command(std::string_view name) {
    std::lock_guard lock(mutex_);
    return handlers_.erase(std::string(name)) != 0;
}

bool CommandRegistry::contains(std::string_view name) const {
    std::lock_guard lock(mutex_);
    return handlers_.find(std::string(name)) != handlers_.end();
}

InvokeResult CommandRegistry::invoke(const InvokeRequest& request) const {
    Handler handler;
    {
        std::lock_guard lock(mutex_);
        const auto found = handlers_.find(request.command);
        if (found == handlers_.end()) {
            return InvokeResult::error(
                error_object("command-not-found", "command '" + request.command + "' is not registered")
            );
        }
        handler = found->second;
    }

    try {
        return handler(request);
    } catch (...) {
        return InvokeResult::error(error_object("handler-failed", "command handler raised an exception"));
    }
}

std::int64_t CounterState::get() const {
    std::lock_guard lock(mutex_);
    return value_;
}

std::int64_t CounterState::increment() {
    std::lock_guard lock(mutex_);
    return ++value_;
}

std::int64_t CounterState::decrement() {
    std::lock_guard lock(mutex_);
    return --value_;
}

std::int64_t CounterState::reset() {
    std::lock_guard lock(mutex_);
    value_ = 0;
    return value_;
}

void register_hello_commands(CommandRegistry& registry, CounterState& counter) {
    registry.register_command("greet", [](const InvokeRequest& request) {
        std::string name;
        std::string error;
        if (!json_string_field(request.payload, "name", name, error)) {
            return InvokeResult::error(error_object("invalid-args", error));
        }
        return InvokeResult::ok(json_quote("Hello " + name + ", You have been greeted from C++!"));
    });

    const auto register_counter_command = [&registry, &counter](std::string name, auto operation) {
        registry.register_command(std::move(name), [&counter, operation](const InvokeRequest& request) {
            if (!json_has_no_arguments(request.payload)) {
                return InvokeResult::error(error_object("invalid-args", "counter commands do not accept arguments"));
            }
            return InvokeResult::ok(std::to_string((counter.*operation)()));
        });
    };

    register_counter_command("get", &CounterState::get);
    register_counter_command("increment", &CounterState::increment);
    register_counter_command("decrement", &CounterState::decrement);
    register_counter_command("reset", &CounterState::reset);
}

IpcRuntime::IpcRuntime(CommandRegistry& registry, std::string runtime_key):
    registry_(registry),
    runtime_key_(std::move(runtime_key)) { }

bool IpcRuntime::register_callback(std::uint64_t id, Callback callback) {
    if (id == 0 || !callback) {
        return false;
    }
    std::lock_guard lock(mutex_);
    return callbacks_.emplace(id, std::move(callback)).second;
}

bool IpcRuntime::unregister_callback(std::uint64_t id) {
    std::lock_guard lock(mutex_);
    return callbacks_.erase(id) != 0;
}

std::size_t IpcRuntime::pending_callback_count() const {
    std::lock_guard lock(mutex_);
    return callbacks_.size();
}

IpcResponse IpcRuntime::dispatch(std::string_view envelope) {
    std::unordered_map<std::string, std::string> fields;
    std::string parse_error;
    JsonParser parser(envelope);
    if (!parser.parse_object(fields, parse_error)) {
        return { false, false, 0, 0, {}, error_object("invalid-envelope", parse_error) };
    }

    IpcResponse response;
    const std::string raw_callback = raw_field(fields, "callback");
    const std::string raw_error_callback = raw_field(fields, "error");
    if (!parse_callback_id(raw_callback, response.callback_id) ||
        !parse_callback_id(raw_error_callback, response.error_callback_id) ||
        response.callback_id == 0 ||
        response.error_callback_id == 0 ||
        response.callback_id == response.error_callback_id) {
        response.error_payload =
            error_object("invalid-envelope", "callback and error ids must be distinct positive integers");
        return response;
    }

    const std::string raw_command = raw_field(fields, "cmd");
    const std::string raw_key = raw_field(fields, "__TAURI_INVOKE_KEY__");
    std::string command;
    std::string invoke_key;
    if (!parse_json_string(raw_command, command) || command.empty()) {
        response.error_payload = error_object("invalid-envelope", "cmd must be a non-empty string");
    } else if (!parse_json_string(raw_key, invoke_key) || invoke_key != runtime_key_) {
        response.error_payload = error_object("invalid-invoke-key", "invoke key is missing or invalid");
    } else {
        const std::string raw_payload = raw_field(fields, "payload");
        const std::string payload = raw_payload.empty() ? "null" : raw_payload;
        InvokeRequest request;
        request.command = std::move(command);
        request.payload = payload;
        const std::string raw_label = raw_field(fields, "webviewLabel");
        const std::string raw_origin = raw_field(fields, "origin");
        parse_json_string(raw_label, request.webview_label);
        parse_json_string(raw_origin, request.origin);
        const InvokeResult result = registry_.invoke(request);
        response.accepted = true;
        response.success = result.success;
        response.payload = result.payload;
        response.error_payload = result.error_payload;
    }

    Callback callback;
    {
        std::lock_guard lock(mutex_);
        const std::uint64_t callback_id = response.success ? response.callback_id : response.error_callback_id;
        const auto found = callbacks_.find(callback_id);
        if (found != callbacks_.end()) {
            callback = found->second;
        }
        callbacks_.erase(response.callback_id);
        callbacks_.erase(response.error_callback_id);
    }
    if (callback) {
        try {
            callback(response);
        } catch (...) {
            // A page callback cannot unwind into the native invoke path.
        }
    }
    return response;
}

} // namespace tauripp