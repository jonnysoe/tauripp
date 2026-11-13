#include "taopp/backend.h"

#include <cassert>
#include <cstdint>

namespace {

void run_task(void* context) { *static_cast<int*>(context) = 42; }

void count_event(void* context, const taopp_backend_event_v1* event) {
    if (event != nullptr) {
        ++*static_cast<int*>(context);
    }
}

} // namespace

int main() {
    const taopp_backend_api_v1* api = taopp_backend_query(TAOPP_BACKEND_ABI_VERSION_1);
    assert(api != nullptr);
    assert(api->abi_version == TAOPP_BACKEND_ABI_VERSION_1);
    assert(api->struct_size >= sizeof(taopp_backend_api_v1));
    assert(api->create != nullptr);
    assert(api->destroy != nullptr);
    assert(api->run_iteration != nullptr);

    taopp_backend_context* context = nullptr;
    const taopp_backend_config_v1 config { sizeof(taopp_backend_config_v1), { nullptr, 0 } };
    assert(api->create(&config, &context) == TAOPP_STATUS_OK);
    assert(context != nullptr);

    int event_count = 0;
    const std::uint64_t subscription = api->subscribe(context, &count_event, &event_count);
    assert(subscription != 0);

    int task_value = 0;
    assert(api->post(context, &run_task, &task_value) == TAOPP_STATUS_OK);
    assert(api->run_iteration(context, 0) == 0);
    assert(task_value == 42);
    assert(event_count > 0);
    assert(api->unsubscribe(context, subscription) == TAOPP_STATUS_OK);

    taopp_window_options_v1 options {};
    options.struct_size = sizeof(options);
    options.title = { "ABI test", 8 };
    options.width = 320;
    options.height = 200;
    std::uint64_t window_id = 0;
    const std::int32_t window_result = api->create_window(context, &options, &window_id);
    assert(window_result == TAOPP_STATUS_OK || window_result == TAOPP_STATUS_UNSUPPORTED);
    if (window_result == TAOPP_STATUS_OK) {
        assert(window_id != 0);
        assert(api->destroy_window(context, window_id) == TAOPP_STATUS_OK);
    }

    api->destroy(context);
    return 0;
}