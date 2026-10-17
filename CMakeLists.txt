cmake_minimum_required(VERSION 3.21)
project(FindEverything VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # lets clangd / VS Code understand the project

option(FE_BUILD_GUI   "Build the ImGui desktop app" ON)
option(FE_BUILD_TESTS "Build unit tests"            ON)

find_package(Threads REQUIRED)

function(fe_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# core: platform-independent index + query + search. No OS headers allowed here.
# ---------------------------------------------------------------------------
add_library(fe_core STATIC
    src/core/Index.cpp
    src/core/Query.cpp
    src/core/Search.cpp
)
target_include_directories(fe_core PUBLIC src)
target_link_libraries(fe_core PUBLIC Threads::Threads)
fe_warnings(fe_core)

# ---------------------------------------------------------------------------
# platform: how we read the filesystem on each OS.
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(FE_PLATFORM_SOURCES
        src/platform/linux/LinuxScanner.cpp
        src/platform/linux/LinuxMounts.cpp
        src/platform/linux/LinuxWatcher.cpp
        src/platform/linux/LinuxShell.cpp
        src/platform/linux/LinuxAppearance.cpp
        src/platform/linux/Process.cpp
    )
else()
    # Windows / macOS use the portable std::filesystem scanner until native
    # backends (NTFS MFT + USN journal, FSEvents) are written. See docs/ROADMAP.md.
    set(FE_PLATFORM_SOURCES
        src/platform/generic/GenericScanner.cpp
        src/platform/generic/GenericWatcher.cpp
        src/platform/generic/GenericShell.cpp
        src/platform/generic/GenericAppearance.cpp
    )
endif()
add_library(fe_platform STATIC ${FE_PLATFORM_SOURCES})
target_link_libraries(fe_platform PUBLIC fe_core)
if(WIN32)
    target_link_libraries(fe_platform PUBLIC shell32 advapi32)
endif()
fe_warnings(fe_platform)

# ---------------------------------------------------------------------------
# fe-cli: terminal front-end, handy for testing and benchmarking.
# ---------------------------------------------------------------------------
add_executable(fe-cli src/cli/main.cpp)
target_link_libraries(fe-cli PRIVATE fe_platform)
fe_warnings(fe-cli)

# ---------------------------------------------------------------------------
# FindEverything: the GUI.
# ---------------------------------------------------------------------------
if(FE_BUILD_GUI)
    include(cmake/Dependencies.cmake)
    add_executable(FindEverything
        src/app/main.cpp
        src/app/MainWindow.cpp
        src/app/Theme.cpp
    )
    target_link_libraries(FindEverything PRIVATE fe_platform imgui)
    fe_warnings(FindEverything)
endif()

if(FE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
