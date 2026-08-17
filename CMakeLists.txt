cmake_minimum_required(VERSION 3.16)
project(sharexl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui DBus Network)
find_package(LayerShellQt)

add_executable(sharexl
    src/main.cpp
    src/capture/CaptureBackend.cpp
    src/capture/OverlayWindow.cpp
    src/capture/SessionState.cpp
    src/services/Config.cpp
    src/services/HotkeyManager.cpp
    src/services/CatboxUploader.cpp
    src/services/Platform.cpp
    src/ui/TrayApp.cpp
    src/ui/SettingsDialog.cpp
    src/ui/HistoryDialog.cpp
    resources/resources.qrc
)

target_include_directories(sharexl PRIVATE src)

target_link_libraries(sharexl PRIVATE Qt6::Widgets Qt6::Gui Qt6::DBus Qt6::Network)

if(LayerShellQt_FOUND)
    target_compile_definitions(sharexl PRIVATE HAVE_LAYERSHELLQT)
    target_link_libraries(sharexl PRIVATE LayerShellQt::Interface)
else()
    message(STATUS "LayerShellQt not found - building without KDE Plasma overlay positioning (falls back to plain fullscreen on other Wayland compositors)")
endif()
