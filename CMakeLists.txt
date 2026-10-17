cmake_minimum_required(VERSION 3.21)
project(FindEverything VERSION 0.2.0 LANGUAGES C CXX)
set(FE_VERSION_SUFFIX "-beta")  # "" for a release

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "FindEverything is Linux only")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FE_BUILD_GUI   "Build the GUI"   ON)
option(FE_BUILD_TESTS "Build the tests" ON)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)  # sd-bus for the tray icon

function(fe_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

# core: index, search, sort, on-disk format. no os stuff in here
add_library(fe_core STATIC
    src/core/Index.cpp
    src/core/IndexFile.cpp
    src/core/PathCache.cpp
    src/core/Query.cpp
    src/core/Search.cpp
    src/core/Sort.cpp
)
target_include_directories(fe_core PUBLIC src)
target_link_libraries(fe_core PUBLIC Threads::Threads)
fe_warnings(fe_core)

# platform: scanning, mounts, xdg, desktop integration
add_library(fe_platform STATIC
    src/platform/Appearance.cpp
    src/platform/Autostart.cpp
    src/platform/FileIcons.cpp
    src/platform/IconTheme.cpp
    src/platform/Inotify.cpp
    src/platform/MimeDatabase.cpp
    src/platform/Mounts.cpp
    src/platform/Paths.cpp
    src/platform/Process.cpp
    src/platform/Scanner.cpp
    src/platform/Shell.cpp
    src/platform/Tray.cpp
    src/platform/Volumes.cpp
)
target_link_libraries(fe_platform PUBLIC fe_core PkgConfig::SYSTEMD)
fe_warnings(fe_platform)

add_executable(fe-cli src/cli/main.cpp src/app/Settings.cpp)
target_link_libraries(fe-cli PRIVATE fe_platform)
fe_warnings(fe-cli)

if(FE_BUILD_GUI)
    include(cmake/Dependencies.cmake)
    include(cmake/EmbedIcon.cmake)
    add_executable(FindEverything
        src/app/main.cpp
        src/app/MainWindow.cpp
        src/app/Theme.cpp
        src/app/Settings.cpp
        src/app/PreferencesDialog.cpp
        src/app/IconCache.cpp
        src/app/IndexService.cpp
        src/app/UpdateChecker.cpp
    )
    target_link_libraries(FindEverything PRIVATE fe_platform imgui image_decoders)
    target_compile_definitions(FindEverything PRIVATE FE_VERSION="${PROJECT_VERSION}${FE_VERSION_SUFFIX}")
    target_include_directories(FindEverything PRIVATE ${CMAKE_BINARY_DIR}/generated)
    fe_warnings(FindEverything)

    # cmake --install build/release --prefix ~/.local  -> shows up in the app menu
    include(GNUInstallDirs)
    install(TARGETS FindEverything fe-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES data/findeverything.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/findeverything.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)

    # cpack -G DEB  (scripts/package.sh builds everything for a release)
    set(CPACK_PACKAGE_NAME "findeverything")
    set(CPACK_PACKAGE_VENDOR "KimsFerdy")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Instant file search for Linux, inspired by Everything")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/KimsFerdy/FindEverything")
    set(CPACK_PACKAGE_CONTACT "KimsFerdy <102543259+KimsFerdy@users.noreply.github.com>")
    set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)  # dpkg-shlibdeps figures out the depends line
    include(CPack)
endif()

if(FE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
