add_library(taopp_core STATIC
    core.cpp
)
target_include_directories(taopp_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

if(WIN32)
    set(TAOPP_GUI_SOURCES backend_windows.cpp)
    set(TAOPP_GUI_LIBRARIES User32 Gdi32 Dwmapi)

    set(TAOPP_WINDOWS_SDK_BASES
        "$ENV{ProgramFiles}/Windows Kits/10/Include"
        "$ENV{ProgramW6432}/Windows Kits/10/Include"
        "C:/Program Files (x86)/Windows Kits/10/Include"
    )
    set(TAOPP_WINDOWS_SDK_INCLUDES)
    foreach(TAOPP_WINDOWS_SDK_BASE IN LISTS TAOPP_WINDOWS_SDK_BASES)
        if(EXISTS "${TAOPP_WINDOWS_SDK_BASE}")
            file(GLOB TAOPP_WINDOWS_SDK_VERSION_INCLUDES LIST_DIRECTORIES true "${TAOPP_WINDOWS_SDK_BASE}/*")
            list(APPEND TAOPP_WINDOWS_SDK_INCLUDES ${TAOPP_WINDOWS_SDK_VERSION_INCLUDES})
        endif()
    endforeach()
    list(SORT TAOPP_WINDOWS_SDK_INCLUDES COMPARE NATURAL ORDER DESCENDING)
    foreach(TAOPP_WINDOWS_SDK_INCLUDE IN LISTS TAOPP_WINDOWS_SDK_INCLUDES)
        if(EXISTS "${TAOPP_WINDOWS_SDK_INCLUDE}/cppwinrt/winrt/base.h" AND
           EXISTS "${TAOPP_WINDOWS_SDK_INCLUDE}/um/windows.ui.xaml.hosting.desktopwindowxamlsource.h")
            set(TAOPP_CPPWINRT_INCLUDE_DIR "${TAOPP_WINDOWS_SDK_INCLUDE}/cppwinrt")
            set(TAOPP_WINDOWS_UM_INCLUDE_DIR "${TAOPP_WINDOWS_SDK_INCLUDE}/um")
            break()
        endif()
    endforeach()

    if(TAOPP_CPPWINRT_INCLUDE_DIR AND TAOPP_WINDOWS_UM_INCLUDE_DIR)
        set(TAOPP_HAS_XAML_ISLANDS ON)
        list(APPEND TAOPP_GUI_LIBRARIES WindowsApp)
        message(STATUS "taopp: enabling Windows XAML Islands")
    else()
        set(TAOPP_HAS_XAML_ISLANDS OFF)
        message(WARNING "taopp: Windows XAML Islands headers were not found; using Win32 controls")
    endif()
else()
    set(TAOPP_GUI_SOURCES backend_stub.cpp)
endif()

add_library(taopp_gui STATIC
    ${TAOPP_GUI_SOURCES}
)
target_link_libraries(taopp_gui PUBLIC taopp_core ${TAOPP_GUI_LIBRARIES})
if(WIN32)
    target_compile_definitions(taopp_gui PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(TAOPP_HAS_XAML_ISLANDS)
        target_compile_definitions(taopp_gui PRIVATE TAOPP_HAS_XAML_ISLANDS=1)
        target_compile_features(taopp_gui PRIVATE cxx_std_20)
        target_include_directories(taopp_gui PRIVATE
            ${TAOPP_CPPWINRT_INCLUDE_DIR}
            ${TAOPP_WINDOWS_UM_INCLUDE_DIR}
        )
    else()
        target_compile_definitions(taopp_gui PRIVATE TAOPP_HAS_XAML_ISLANDS=0)
    endif()
endif()

add_library(taopp_host STATIC
    backend.cpp
)
target_include_directories(taopp_host
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(taopp_host PUBLIC taopp_core taopp_gui)