cmake_minimum_required(VERSION 3.21)
project(plasma_spotify_controls VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core DBus Gui Network Qml Test)
find_package(KF6Wallet REQUIRED)
include(GNUInstallDirs)
find_program(MSGFMT_EXECUTABLE msgfmt REQUIRED)
# Plasma's QML engine uses Qt's runtime import root (/usr/lib/qt/qml on this distro),
# not the Qt development-version directory (for example, lib/qt6/qml).
add_library(spotifyplasmoidplugin MODULE
    src/spotifycontroller.h src/spotifycontroller.cpp
    src/spotifyplasmoidplugin.h src/spotifyplasmoidplugin.cpp
)
target_link_libraries(spotifyplasmoidplugin PRIVATE Qt6::Core Qt6::DBus Qt6::Gui Qt6::Network Qt6::Qml KF6::Wallet)
target_include_directories(spotifyplasmoidplugin PRIVATE src)

install(TARGETS spotifyplasmoidplugin
    LIBRARY DESTINATION "${CMAKE_INSTALL_DATADIR}/plasma/plasmoids/com.github.spotifyplasmoid/contents/code/spotifybridge"
    RUNTIME DESTINATION "${CMAKE_INSTALL_DATADIR}/plasma/plasmoids/com.github.spotifyplasmoid/contents/code/spotifybridge")
install(DIRECTORY package/ DESTINATION "${CMAKE_INSTALL_DATADIR}/plasma/plasmoids/com.github.spotifyplasmoid")
install(FILES assets/spotify-companion.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/plasma_applet_com.github.spotifyplasmoid.mo"
    COMMAND "${MSGFMT_EXECUTABLE}" -o "${CMAKE_CURRENT_BINARY_DIR}/plasma_applet_com.github.spotifyplasmoid.mo" "${CMAKE_CURRENT_SOURCE_DIR}/po/tr/spotifyplasmoid.po"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/po/tr/spotifyplasmoid.po")
add_custom_target(spotifyplasmoid_translations ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/plasma_applet_com.github.spotifyplasmoid.mo")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/plasma_applet_com.github.spotifyplasmoid.mo"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/locale/tr/LC_MESSAGES")

enable_testing()
qt_add_executable(test_trackuri tests/test_trackuri.cpp src/spotifycontroller.cpp)
target_link_libraries(test_trackuri PRIVATE Qt6::Core Qt6::DBus Qt6::Gui Qt6::Network Qt6::Test KF6::Wallet)
target_include_directories(test_trackuri PRIVATE src)
add_test(NAME trackuri COMMAND test_trackuri)
