add_library(tauripp_core STATIC
    assets.cpp
    core.cpp
)
target_include_directories(tauripp_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_library(tauripp_host STATIC
    host.cpp
)
target_include_directories(tauripp_host
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(tauripp_host PUBLIC tauripp_core taopp_host)

add_library(tauripp STATIC
    framework.cpp
)
target_include_directories(tauripp
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(tauripp PUBLIC tauripp_host)

install(TARGETS tauripp
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)