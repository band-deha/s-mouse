find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

add_executable(smouse-gui
    main.cpp
    mainwindow.cpp
    screen_editor.cpp
    settings_dialog.cpp
    tray_icon.cpp
    ../core/server.cpp
    ../core/client.cpp
)

target_include_directories(smouse-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(smouse-gui PRIVATE
    Qt6::Widgets
    smouse_core
    smouse_net
    smouse_platform
)

if(APPLE)
    target_link_libraries(smouse-gui PRIVATE "-framework OpenGL")

    # AGL framework was removed from newer macOS SDKs but Qt6 still references it.
    # Build a stub dylib and embed it inside the .app bundle so the runtime linker finds it.
    set(AGL_STUB_DIR "${CMAKE_BINARY_DIR}/stubs/AGL.framework")
    if(NOT EXISTS "${AGL_STUB_DIR}/AGL")
        file(MAKE_DIRECTORY "${AGL_STUB_DIR}")
        file(WRITE "${CMAKE_BINARY_DIR}/stubs/agl_stub.c" "/* AGL stub */\n")
        execute_process(
            COMMAND cc -dynamiclib -o "${AGL_STUB_DIR}/AGL"
                    "${CMAKE_BINARY_DIR}/stubs/agl_stub.c"
                    -install_name "@rpath/AGL.framework/AGL"
        )
    endif()
    # Let the linker find the stub at build time
    target_link_options(smouse-gui PRIVATE "-F${CMAKE_BINARY_DIR}/stubs")

    # Copy the stub into the app bundle so the runtime linker finds it too
    add_custom_command(TARGET smouse-gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:smouse-gui>/Contents/Frameworks/AGL.framework"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${AGL_STUB_DIR}/AGL"
            "$<TARGET_BUNDLE_DIR:smouse-gui>/Contents/Frameworks/AGL.framework/AGL"
        COMMAND install_name_tool -add_rpath
            "@executable_path/../Frameworks"
            "$<TARGET_BUNDLE_DIR:smouse-gui>/Contents/MacOS/smouse-gui"
            || true
    )
endif()

set_target_properties(smouse-gui PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER com.smouse.gui
    MACOSX_BUNDLE_BUNDLE_NAME "S-Mouse"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
)
