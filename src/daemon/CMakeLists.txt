add_executable(smouse-daemon
    main.cpp
    ../core/server.cpp
    ../core/client.cpp
)

target_include_directories(smouse-daemon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(smouse-daemon PRIVATE
    smouse_core
    smouse_net
    smouse_platform
)
