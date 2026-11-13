find_program(GIT_EXE git)

function(git_clone)
    # Define schema:
    # - no boolean flags in this example
    # - one-value keywords
    # - multi-value keywords (none here)
    set(options)
    set(oneValueArgs REPO_URL BRANCH_NAME TARGET_DIR WORKING_DIRECTORY RESULT_VARIABLE)
    set(multiValueArgs)

    cmake_parse_arguments(GC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Defaults
    if(NOT GC_BRANCH_NAME)
        set(_branch_arg main)
    else()
        set(_branch_arg ${GC_BRANCH_NAME})
    endif()

    if(NOT GC_TARGET_DIR)
        string(REPLACE "/" ";" repo_url_list "${GC_REPO_URL}")
        list(GET repo_url_list -1 basename)

        # Remove all extensions from basename (e.g., .git)
        string(REPLACE "." ";" basename "${basename}")
        list(GET basename 0 GC_TARGET_DIR)
    endif()

    if(NOT GC_REPO_URL)
        message(FATAL_ERROR "REPO_URL is required")
    endif()

    # Optional working dir
    set(_wd_args)
    if(GC_WORKING_DIRECTORY)
        list(APPEND _wd_args WORKING_DIRECTORY "${GC_WORKING_DIRECTORY}")
        set(_dircheck ${GC_WORKING_DIRECTORY}/${GC_TARGET_DIR})
    else()
        list(APPEND _wd_args WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set(_dircheck ${CMAKE_CURRENT_SOURCE_DIR}/${GC_TARGET_DIR})
    endif()

    set(_clone_result 0)

    file(GLOB DIR_FILES ${_dircheck}/*)
    list(LENGTH DIR_FILES DIR_LEN)
    if(DIR_LEN EQUAL 0)
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" _shortdir "${_dircheck}")
        if(NOT GIT_EXE)
            message(FATAL_ERROR "Missing ${_shortdir} content and git is not installed to retrieve them.")
        endif()

        message(STATUS "Cloning ${GC_REPO_URL} into ${_shortdir}...")
        execute_process(
            COMMAND "${GIT_EXE}" clone --single-branch --branch "${_branch_arg}" "${GC_REPO_URL}" "${GC_TARGET_DIR}"
            ${_wd_args}
            RESULT_VARIABLE _clone_result
            OUTPUT_QUIET
            ERROR_QUIET
        )

        if(NOT _clone_result EQUAL 0 AND NOT GC_BRANCH_NAME)
            # If BRANCH_NAME was not specified, because "main" failed so retry with "master"
            set(_branch_arg master)
            execute_process(
                COMMAND "${GIT_EXE}" clone --single-branch --branch "${_branch_arg}" "${GC_REPO_URL}" "${GC_TARGET_DIR}"
                ${_wd_args}
                RESULT_VARIABLE _clone_result
                OUTPUT_QUIET
                ERROR_QUIET
            )
        endif()
    endif()

    # Optional output variable name
    if(GC_RESULT_VARIABLE)
        set(${GC_RESULT_VARIABLE} "${_clone_result}" PARENT_SCOPE)
    endif()
endfunction()