add_library(smouse_core
    protocol.cpp
    keymap.cpp
    screen_layout.cpp
    clipboard.cpp
)

target_include_directories(smouse_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
