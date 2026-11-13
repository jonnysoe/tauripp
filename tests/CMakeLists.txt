set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(TAURIPP_RUNTIME_TARGETS taopp_gui taopp_host tauripp_host)

function(tauripp_configure_test_runtime test_name test_target)
    set(path_modifications "PATH=path_list_prepend:$<TARGET_FILE_DIR:${test_target}>")
    foreach(runtime_target IN LISTS TAURIPP_RUNTIME_TARGETS)
        if(TARGET ${runtime_target})
            list(APPEND path_modifications "PATH=path_list_prepend:$<TARGET_FILE_DIR:${runtime_target}>")
        endif()
    endforeach()
    set_tests_properties(${test_name} PROPERTIES ENVIRONMENT_MODIFICATION "${path_modifications}")
endfunction()

add_executable(taopp_core_tests
    taopp_core.cpp
)
target_link_libraries(taopp_core_tests PRIVATE taopp_core)
add_test(NAME tauripp.taopp.core COMMAND taopp_core_tests)
tauripp_configure_test_runtime(tauripp.taopp.core taopp_core_tests)

add_executable(tauripp_core_tests
    tauripp_core.cpp
)
target_link_libraries(tauripp_core_tests PRIVATE tauripp_core)
add_test(NAME tauripp.hello.core COMMAND tauripp_core_tests hello)
add_test(NAME tauripp.buttons.core COMMAND tauripp_core_tests buttons)
add_test(NAME tauripp.hello-buttons.core COMMAND tauripp_core_tests hello-buttons)
tauripp_configure_test_runtime(tauripp.hello.core tauripp_core_tests)
tauripp_configure_test_runtime(tauripp.buttons.core tauripp_core_tests)
tauripp_configure_test_runtime(tauripp.hello-buttons.core tauripp_core_tests)

add_executable(tauripp_ipc_tests
    ipc_contract.cpp
)
target_link_libraries(tauripp_ipc_tests PRIVATE tauripp_core)
add_test(NAME tauripp.ipc.hello COMMAND tauripp_ipc_tests)
tauripp_configure_test_runtime(tauripp.ipc.hello tauripp_ipc_tests)

add_executable(tauripp_asset_tests
    asset_protocol.cpp
)
target_link_libraries(tauripp_asset_tests PRIVATE tauripp_core)
add_test(NAME tauripp.assets.hello COMMAND tauripp_asset_tests)
set_tests_properties(tauripp.assets.hello PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
tauripp_configure_test_runtime(tauripp.assets.hello tauripp_asset_tests)

add_executable(tauripp_backend_tests
    backend_contract.cpp
)
target_link_libraries(tauripp_backend_tests PRIVATE taopp_host)
tauripp_enable_windows_manifest(tauripp_backend_tests)
add_test(NAME tauripp.taopp.abi COMMAND tauripp_backend_tests)
tauripp_configure_test_runtime(tauripp.taopp.abi tauripp_backend_tests)