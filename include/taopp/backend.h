#ifndef TAOPP_BACKEND_H
#define TAOPP_BACKEND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TAOPP_BACKEND_ABI_VERSION_1 1u

enum {
    TAOPP_STATUS_OK = 0,
    TAOPP_STATUS_INVALID_ARGUMENT = -1,
    TAOPP_STATUS_UNSUPPORTED = -2,
    TAOPP_STATUS_SYSTEM_ERROR = -3,
};

enum {
    TAOPP_CAP_EVENT_LOOP = UINT64_C(1) << 0,
    TAOPP_CAP_WINDOWS = UINT64_C(1) << 1,
    TAOPP_CAP_NATIVE_CONTROLS = UINT64_C(1) << 2,
};

enum {
    TAOPP_EVENT_NEW_EVENTS = 0,
    TAOPP_EVENT_WINDOW = 1,
    TAOPP_EVENT_USER = 2,
    TAOPP_EVENT_MAIN_EVENTS_CLEARED = 3,
    TAOPP_EVENT_REDRAW_REQUESTED = 4,
    TAOPP_EVENT_LOOP_DESTROYED = 5,
};

typedef struct taopp_backend_context taopp_backend_context;

typedef struct taopp_utf8_span {
    const char* data;
    uint64_t size;
} taopp_utf8_span;

typedef struct taopp_backend_config_v1 {
    uint32_t struct_size;
    taopp_utf8_span application_name;
} taopp_backend_config_v1;

typedef struct taopp_window_options_v1 {
    uint32_t struct_size;
    taopp_utf8_span title;
    int32_t width;
    int32_t height;
    int32_t x;
    int32_t y;
    uint32_t flags;
} taopp_window_options_v1;

typedef struct taopp_backend_event_v1 {
    uint32_t struct_size;
    uint32_t type;
    uint64_t window_id;
    int32_t width;
    int32_t height;
    int32_t x;
    int32_t y;
    float scale_factor;
    uint64_t user_id;
    taopp_utf8_span user_payload;
} taopp_backend_event_v1;

typedef void (*taopp_backend_event_callback_v1)(void* context, const taopp_backend_event_v1* event);
typedef void (*taopp_backend_task_callback_v1)(void* context);

typedef int32_t (*taopp_backend_create_fn)(const taopp_backend_config_v1* config, taopp_backend_context** context);
typedef void (*taopp_backend_destroy_fn)(taopp_backend_context* context);
typedef int32_t (*taopp_backend_run_fn)(taopp_backend_context* context);
typedef int32_t (*taopp_backend_run_iteration_fn)(taopp_backend_context* context, uint32_t wait_milliseconds);
typedef int32_t (*taopp_backend_post_fn)(
    taopp_backend_context* context,
    taopp_backend_task_callback_v1 callback,
    void* callback_context
);
typedef int32_t (*taopp_backend_create_window_fn)(
    taopp_backend_context* context,
    const taopp_window_options_v1* options,
    uint64_t* window_id
);
typedef int32_t (*taopp_backend_destroy_window_fn)(taopp_backend_context* context, uint64_t window_id);
typedef uint64_t (*taopp_backend_subscribe_fn)(
    taopp_backend_context* context,
    taopp_backend_event_callback_v1 callback,
    void* callback_context
);
typedef int32_t (*taopp_backend_unsubscribe_fn)(taopp_backend_context* context, uint64_t callback_id);

typedef struct taopp_backend_api_v1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t capability_bits;
    const char* name;
    taopp_backend_create_fn create;
    taopp_backend_destroy_fn destroy;
    taopp_backend_run_fn run;
    taopp_backend_run_iteration_fn run_iteration;
    taopp_backend_post_fn post;
    taopp_backend_create_window_fn create_window;
    taopp_backend_destroy_window_fn destroy_window;
    taopp_backend_subscribe_fn subscribe;
    taopp_backend_unsubscribe_fn unsubscribe;
} taopp_backend_api_v1;

const taopp_backend_api_v1* taopp_backend_query(uint32_t requested_abi);

#ifdef __cplusplus
}
#endif

#endif