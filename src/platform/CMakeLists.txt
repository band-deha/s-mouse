add_library(smouse_platform)

target_include_directories(smouse_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(smouse_platform PUBLIC smouse_core)

if(APPLE)
    target_sources(smouse_platform PRIVATE
        macos/macos_input_capture.mm
        macos/macos_input_inject.mm
        macos/macos_screen.mm
        macos/macos_clipboard.mm
        macos/macos_ble.mm
    )
    target_link_libraries(smouse_platform PRIVATE
        "-framework ApplicationServices"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework AppKit"
        "-framework CoreBluetooth"
    )
elseif(WIN32)
    target_sources(smouse_platform PRIVATE
        windows/win_input_capture.cpp
        windows/win_input_inject.cpp
        windows/win_screen.cpp
        windows/win_clipboard.cpp
    )
endif()
