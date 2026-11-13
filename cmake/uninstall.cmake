# https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#can-i-do-make-uninstall-with-cmake
if(EXISTS "${BINARY_DIR}/install_manifest.txt")
    message(STATUS "Uninstalling from install manifest: ${BINARY_DIR}/install_manifest.txt")

    file(READ "${BINARY_DIR}/install_manifest.txt" files)
    string(REGEX REPLACE "\n" ";" files "${files}")
    set(dir "")
    foreach(file ${files})
        message(STATUS "Uninstalling: ${file}")
        if(IS_SYMLINK "${file}" OR EXISTS "${file}")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E remove "${file}"
                OUTPUT_VARIABLE rm_out
                RESULT_VARIABLE rm_retval
            )
            if(NOT "${rm_retval}" STREQUAL 0)
                message(FATAL_ERROR "Problem when removing ${file}")
            endif()
        else(IS_SYMLINK "${file}" OR EXISTS "${file}")
            message(STATUS "File ${file} does not exist.")
        endif()

        cmake_path(GET file PARENT_PATH dir)
        list(APPEND dirs ${dir})
    endforeach()

    list(REMOVE_DUPLICATES dirs)
    foreach(dir ${dirs})
        # Check if directory exist first just in case install_manifest.txt is out of order
        # where parent directory alreaady recursively removed this directory
        while(EXISTS ${dir})
            file(GLOB RESULT ${dir}/*)
            list(LENGTH RESULT DIR_LEN)
            if(DIR_LEN GREATER 0)
                # ${dir} is not empty!
                break()
            endif()
            # ${dir} is empty, remoove it
            file(REMOVE_RECURSE ${dir})

            # iterate with parent directory
            cmake_path(GET dir PARENT_PATH dir)
        endwhile()
    endforeach()

    file(REMOVE "${BINARY_DIR}/install_manifest.txt")

else()
    message(WARNING "Not installed, there is no install manifest: ${BINARY_DIR}/install_manifest.txt")
endif()
