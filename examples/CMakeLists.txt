function(add_tauripp_example target source)
    add_executable(${target}
        common.cpp
        ${source}
    )
    target_link_libraries(${target} PRIVATE tauripp)
    tauripp_enable_windows_manifest(${target})
    install(TARGETS ${target}
        RUNTIME DESTINATION .
    )
endfunction()

add_tauripp_example(tauripp_hello hello/main.cpp)
add_tauripp_example(tauripp_buttons buttons/main.cpp)
add_tauripp_example(tauripp_hello_buttons hello-buttons/main.cpp)