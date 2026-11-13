cmake_minimum_required(VERSION 3.22)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(git_clone)
project(tauripp LANGUAGES C CXX)

if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TAURIPP_FETCH_YYJSON "Fetch yyjson for a future high-throughput IPC backend" OFF)
if(TAURIPP_FETCH_YYJSON)
    git_clone(
        REPO_URL https://github.com/ibireme/yyjson.git
        BRANCH_NAME 0.12.0
        TARGET_DIR third_party/yyjson
    )
endif()

option(TAURIPP_BUILD_EXAMPLES "Build the native tauripp hello-world GUI examples" ON)

function(tauripp_enable_windows_manifest target)
    if(WIN32)
        set(manifest_path "${PROJECT_SOURCE_DIR}/cmake/tauripp.manifest")
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${manifest_path}")
        target_link_options(${target} PRIVATE
            "LINKER:/MANIFEST:EMBED"
            "LINKER:/MANIFESTINPUT:${manifest_path}"
        )
    endif()
endfunction()

if(PROJECT_IS_TOP_LEVEL)
    include(CTest)

    # Check if compiler supports IPO then enable IPO
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT error)
    if(ipo_supported)
        message(STATUS "IPO is will be enabled for release builds.")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)
    endif()

    # Enable CCache
    # https://stackoverflow.com/a/64600661/19336104
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    endif()

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)

    # Check if current session is multiconfig
    get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(NOT IS_MULTI_CONFIG)
        # CMake will default to Debug if no build type is specified for single-config generators
        if(NOT DEFINED CMAKE_BUILD_TYPE)
            set(CMAKE_BUILD_TYPE Release)
        endif()
    endif()
endif()

if(NOT MSVC)
    # Make symbols hidden by default in gcc/clang Linux, as they are default exported which make LTO less effective.
    # -fvisibility=hidden
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
endif()

add_subdirectory(src)

if(TAURIPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# add uninstall target because CMake does not provide one by default
# NOTE: Cannot pass in CMAKE_BINARY_DIR as it will be overwritten, so we pass in as BINARY_DIR instead
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -D BINARY_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake
)

# add format target to run clang-format on all source files
add_custom_target(format
    COMMAND ${CMAKE_COMMAND} -D BINARY_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/clang_format.cmake
)

install(TARGETS taopp_core taopp_gui taopp_host tauripp_core tauripp_host tauripp
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(
    DIRECTORY include/
    DESTINATION include
)

install(
    DIRECTORY assets
    DESTINATION .
)