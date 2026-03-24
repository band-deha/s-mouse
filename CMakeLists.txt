cmake_minimum_required(VERSION 3.21)

if(APPLE)
    project(s-mouse VERSION 0.1.0 LANGUAGES CXX OBJCXX)
else()
    project(s-mouse VERSION 0.1.0 LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(SMOUSE_BUILD_TESTS "Build unit tests" ON)
option(SMOUSE_BUILD_GUI "Build Qt6 GUI application" OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        $<$<CXX_COMPILER_ID:Clang>:-Wno-unused-parameter>
        $<$<CXX_COMPILER_ID:GNU>:-Wno-unused-parameter>
    )
endif()

# Core library (platform-independent)
add_subdirectory(src/core)

# Networking library
add_subdirectory(src/net)

# Platform abstraction
add_subdirectory(src/platform)

# Daemon executable
add_subdirectory(src/daemon)

# GUI application
if(SMOUSE_BUILD_GUI)
    add_subdirectory(src/gui)
endif()

# Tests
if(SMOUSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
