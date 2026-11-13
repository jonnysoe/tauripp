#include "taopp/backend.h"

#include "taopp/native.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>

struct taopp_backend_context {
    taopp::EventLoop event_loop;
    taopp::NativeApplication native;

    taopp_backend_context(): event_loop(), native(event_loop) { }
};

namespace {

constexpr std::uint32_t kVisible = 1u << 0;
constexpr std::uint32_t kResizable = 1u << 1;
constexpr std::uint32_t kDecorated = 1u << 2;
constexpr std::uint32_t kFocused = 1u << 3;

std::string span_to_string(taopp_utf8_span span) {
    if (span.data == nullptr || span.size == 0) {
        return {};
    }
    return { span.data, static_cast<std::size_t>(span.size) };
}

int32_t native_status(const taopp::NativeResult& result) {
    if (result) {
        return TAOPP_STATUS_OK;
    }
    switch (result.code) {
    case taopp::NativeErrorCode::InvalidArgument:
        return TAOPP_STATUS_INVALID_ARGUMENT;
    case taopp::NativeErrorCode::Unsupported:
        return TAOPP_STATUS_UNSUPPORTED;
    default:
        return TAOPP_STATUS_SYSTEM_ERROR;
    }
}

int32_t backend_create(const taopp_backend_config_v1*, taopp_backend_context** output) {
    if (output == nullptr) {
        return TAOPP_STATUS_INVALID_ARGUMENT;
    }
    try {
        *output = new taopp_backend_context();
        return TAOPP_STATUS_OK;
    } catch (const std::bad_alloc&) {
        *output = nullptr;
        return TAOPP_STATUS_SYSTEM_ERROR;
    } catch (...) {
        *output = nullptr;
        return TAOPP_STATUS_SYSTEM_ERROR;
    }
}

void backend_destroy(taopp_backend_context* context) { delete context; }

int32_t backend_run(taopp_backend_context* context) {
    if (context == nullptr) {
        return TAOPP_STATUS_INVALID_ARGUMENT;
    }
    try {
        return context->native.run();
    } catch (...) {
        return TAOPP_STATUS_SYSTEM_ERROR;
    }
}

int32_t backend_run_iteration(taopp_backend_context* context, uint32_t wait_milliseconds) {
    if (context == nullptr) {
        return TAOPP_STATUS_INVALID_ARGUMENT;
    }
    try {
        context->event_loop.run_iteration(std::chrono::milliseconds(wait_milliseconds));
        return context->event_loop.exit_code();
    } catch (...) {
        return TAOPP_STATUS_SYSTEM_ERROR;
    }
}

int32_t backend_post(taopp_backend_context* context, taopp_backend_task_callback_v1 callback, void* callback_context) {
    if (context == nullptr || callback == nullptr) {
        return TAOPP_STATUS_INVALID_ARGUMENT;
    }
    try {
        return context->event_loop.post([callback, callback_context] { callback(callback_context); })
                   ? TAOPP_STATUS_OK
                   : TAOPP_STATUS_SYSTEM_ERROR;
    } catch (...) {
        return TAOPP_STATUS_SYSTEM_ERROR;
    }
}

int32_t
backend_create_window(taopp_backend_context* context, const taopp_window_options_v1* options, uint64_t* window_id) {
    if (context == nullptr || window_id == nullptr) {
        return TAOPP_STATUS_INVALID_ARGUMENT;
    }

    taopp::WindowOptions native_options;
    if (options != nullptr) {
        if (options->struct_size < sizeof(taopp_window_options_v1)) {
            return TAOPP_STATUS_INVALID_ARGUMENT;
        }
        native_options.title = span_to_string(options->title);
        native_options.logical_size = { options->width, options->height };
        native_options.logical_position = { options->x, options->y };
        native_options.has_position = options->x != 0 || options->y != 0;
        native_options.visible = (options->flags & kVisible) != 0;
        native_options.resizable = (options->flags & kResizable) != 0;
        native_options.decorated = (options->flags & kDecorated) != 0;
        native_options.focused = (options->flags & kFocused) != 0;
    }

    try {
        return native_status(context->native.create_window(native_options, *window_id));
    } catch (...) {
        return TAOPP_STATUS_SYSTEM_ERROR;
    }
}

int32_t backend_destroy_window(taopp_backend_context* context, uint64_t window_id) {
    if (context == nullptr) {
        return TAOPP_STATUS_INVALID_ARGUMENT;
    }
    try {
        return native_status(context->native.destroy_window(window_id));
    } catch (...) {
        return TAOPP_STATUS_SYSTEM_ERROR;
    }
}

uint32_t event_type(const taopp::Event& event) {
    switch (event.type) {
    case taopp::EventType::Window:
        return TAOPP_EVENT_WINDOW;
    case taopp::EventType::User:
        return TAOPP_EVENT_USER;
    case taopp::EventType::MainEventsCleared:
        return TAOPP_EVENT_MAIN_EVENTS_CLEARED;
    case taopp::EventType::RedrawRequested:
        return TAOPP_EVENT_REDRAW_REQUESTED;
    case taopp::EventType::LoopDestroyed:
        return TAOPP_EVENT_LOOP_DESTROYED;
    default:
        return TAOPP_EVENT_NEW_EVENTS;
    }
}

uint64_t
backend_subscribe(taopp_backend_context* context, taopp_backend_event_callback_v1 callback, void* callback_context) {
    if (context == nullptr || callback == nullptr) {
        return 0;
    }
    try {
        return context->event_loop.subscribe([callback, callback_context](const taopp::Event& event) {
            taopp_backend_event_v1 native_event {};
            native_event.struct_size = sizeof(native_event);
            native_event.type = event_type(event);
            native_event.window_id = event.window.window;
            native_event.width = event.window.size.width;
            native_event.height = event.window.size.height;
            native_event.x = event.window.position.x;
            native_event.y = event.window.position.y;
            native_event.scale_factor = event.window.scale_factor;
            native_event.user_id = event.user_id;
            native_event.user_payload = { event.user_payload.data(), event.user_payload.size() };
            callback(callback_context, &native_event);
        });
    } catch (...) {
        return 0;
    }
}

int32_t backend_unsubscribe(taopp_backend_context* context, uint64_t callback_id) {
    if (context == nullptr || callback_id == 0) {
        return TAOPP_STATUS_INVALID_ARGUMENT;
    }
    return context->event_loop.unsubscribe(callback_id) ? TAOPP_STATUS_OK : TAOPP_STATUS_INVALID_ARGUMENT;
}

#if defined(_WIN32)
constexpr uint64_t kCapabilities = TAOPP_CAP_EVENT_LOOP | TAOPP_CAP_WINDOWS | TAOPP_CAP_NATIVE_CONTROLS;
constexpr char kBackendName[] = "taopp-win32";
#else
constexpr uint64_t kCapabilities = TAOPP_CAP_EVENT_LOOP;
constexpr char kBackendName[] = "taopp-stub";
#endif

const taopp_backend_api_v1 kBackendApi {
    TAOPP_BACKEND_ABI_VERSION_1,
    sizeof(taopp_backend_api_v1),
    kCapabilities,
    kBackendName,
    &backend_create,
    &backend_destroy,
    &backend_run,
    &backend_run_iteration,
    &backend_post,
    &backend_create_window,
    &backend_destroy_window,
    &backend_subscribe,
    &backend_unsubscribe,
};

} // namespace

extern "C" const taopp_backend_api_v1* taopp_backend_query(uint32_t requested_abi) {
    if (requested_abi != 0 && requested_abi != TAOPP_BACKEND_ABI_VERSION_1) {
        return nullptr;
    }
    return &kBackendApi;
}