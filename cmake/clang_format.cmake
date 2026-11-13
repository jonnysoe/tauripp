find_program(CLANG_FORMAT_EXE
    NAMES clang-format
    REQUIRED
)

if(NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "BINARY_DIR must be provided when running clang_format.cmake.")
endif()

cmake_path(GET BINARY_DIR PARENT_PATH PROJECT_ROOT)

file(GLOB_RECURSE FORMAT_FILES
    LIST_DIRECTORIES false
    "${PROJECT_ROOT}/*.c"
    "${PROJECT_ROOT}/*.cc"
    "${PROJECT_ROOT}/*.cpp"
    "${PROJECT_ROOT}/*.cxx"
    "${PROJECT_ROOT}/*.h"
    "${PROJECT_ROOT}/*.hh"
    "${PROJECT_ROOT}/*.hpp"
    "${PROJECT_ROOT}/*.hxx"
)

list(FILTER FORMAT_FILES EXCLUDE REGEX "/(build|third_party|temp|tmp)(/|$)")

if(NOT FORMAT_FILES)
    message(STATUS "No C/C++ files found to format.")
    return()
endif()

execute_process(
    COMMAND "${CLANG_FORMAT_EXE}" -i ${FORMAT_FILES}
    WORKING_DIRECTORY "${PROJECT_ROOT}"
    RESULT_VARIABLE FORMAT_RESULT
)

if(NOT FORMAT_RESULT EQUAL 0)
    message(FATAL_ERROR "clang-format failed with exit code ${FORMAT_RESULT}.")
endif()