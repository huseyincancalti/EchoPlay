cmake_minimum_required(VERSION 3.21)
project(EchoPlay VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 0.0.0-dev (a dev build) never runs the update check.
set(ECHOPLAY_VERSION "0.0.0-dev" CACHE STRING "Release version without the v; the release build sets it")

find_package(Qt6 6.9 REQUIRED COMPONENTS Quick Gui GuiPrivate Network Test ShaderTools)
qt_standard_project_setup(REQUIRES 6.7)

# libmpv dev package: include/mpv/*.h, libmpv.dll.a, libmpv-2.dll (tools/setup-dev.sh fetches it).
set(MPV_DIR "$ENV{MPV_DIR}" CACHE PATH "libmpv dev package directory")
if(NOT EXISTS "${MPV_DIR}/include/mpv/render.h")
    message(FATAL_ERROR "libmpv not found in MPV_DIR='${MPV_DIR}'. Run tools/setup-dev.sh and source .out/devenv.sh.")
endif()

# The player (QML module: Main.qml and every C++ type) as a static library, so the app and the end-to-end
# test (tests/tst_e2e.cpp) link the same player.
qt_add_library(echoplay_lib STATIC)
qt_add_qml_module(echoplay_lib
    URI EchoPlay
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/EchoPlay" # qmldir path must match the URI (qmllint, tooling)
    QML_FILES qml/Main.qml qml/Icons.js qml/Format.js qml/Icon.qml qml/Label.qml qml/FocusRing.qml qml/IconButton.qml
              qml/TextButton.qml qml/ToggleSwitch.qml qml/Segmented.qml qml/TrackSlider.qml qml/GlassBackdrop.qml
              qml/GlassPanel.qml qml/RoundedImage.qml qml/Popover.qml qml/PlayerUi.qml qml/TitleBar.qml qml/ControlBar.qml
              qml/SeekBar.qml qml/VolumeSlider.qml qml/VolumeFlash.qml qml/ArrowSweep.qml qml/MixerPopover.qml
              qml/StartScreen.qml qml/DashedFrame.qml qml/DropOverlay.qml qml/OpenDialogs.qml qml/Stepper.qml
              qml/QuickSettings.qml qml/SubtitlesPopover.qml qml/SettingRow.qml qml/Dropdown.qml qml/ColorPicker.qml
              qml/SettingsOverlay.qml qml/SettingsContent.qml qml/UpdateCard.qml qml/ErrorScreen.qml qml/UpdateNotice.qml
              qml/Toast.qml qml/ResumeCard.qml qml/HintCard.qml qml/NoSoundCard.qml qml/PlaylistPanel.qml
    SOURCES src/MpvItem.h src/MpvItem.cpp src/SingleInstance.h src/SingleInstance.cpp
            src/I18n.h src/I18n.cpp src/Theme.h src/Theme.cpp src/AppState.h src/AppState.cpp
            src/UpdateCheck.h src/UpdateCheck.cpp src/Updater.h src/Updater.cpp
            src/ResumeStore.h src/ResumeStore.cpp
            src/Mixer.h src/Mixer.cpp src/MixerController.h src/MixerController.cpp
            src/PictureQuality.h src/PictureQuality.cpp src/Thumbnailer.h src/Thumbnailer.cpp
            src/SilenceCheck.h src/SilenceCheck.cpp src/PlayerSettings.h src/PlayerSettings.cpp
            src/Services.h src/Services.cpp src/WindowChrome.h src/WindowChrome.cpp src/MediaFiles.h src/MediaFiles.cpp
            src/ErrorReport.h src/ErrorReport.cpp src/ClipInfo.h src/ClipInfo.cpp
)
qt_add_shaders(echoplay_lib "shaders" PREFIX "/shaders" BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders" FILES shaders/glass.frag)
if(WIN32)
    target_link_libraries(echoplay_lib PRIVATE dwmapi)
endif()
qt_add_executable(echoplay src/main.cpp)
# EchoPlay.exe, as v1's: "Open with", Task Manager and the installer's registry entries use this name.
set_target_properties(echoplay PROPERTIES OUTPUT_NAME EchoPlay)
if(WIN32)
    string(REGEX MATCH "^([0-9]+)[.]([0-9]+)[.]([0-9]+)" _ "${ECHOPLAY_VERSION}")
    set(ECHOPLAY_VERSION_RC "${CMAKE_MATCH_1},${CMAKE_MATCH_2},${CMAKE_MATCH_3},0")
    set(ECHOPLAY_ICON "${CMAKE_CURRENT_SOURCE_DIR}/../assets/logo.ico")
    configure_file(echoplay.rc.in echoplay.rc @ONLY)
    target_sources(echoplay PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/echoplay.rc")
endif()

# libmpv headers in one folder: the dev package's, plus upstream render_d3d11.h that it does not ship
# yet (third_party/mpv/README.md). render_d3d11.h includes "render.h" from its own folder.
set(MPV_INCLUDE "${CMAKE_BINARY_DIR}/mpv-include")
file(COPY "${MPV_DIR}/include/mpv" DESTINATION "${MPV_INCLUDE}")
if(NOT EXISTS "${MPV_INCLUDE}/mpv/render_d3d11.h")
    file(COPY third_party/mpv/mpv/render_d3d11.h DESTINATION "${MPV_INCLUDE}/mpv")
endif()
# src/ for the generated QML type registration, which includes MpvItem.h by bare name.
target_include_directories(echoplay_lib PUBLIC src "${MPV_INCLUDE}")
# Qt6::GuiPrivate: QRhi native handles (rhi/qrhi.h), to hand Qt's D3D11 device to libmpv.
target_link_libraries(echoplay_lib PUBLIC Qt6::Quick Qt6::GuiPrivate Qt6::Network "${MPV_DIR}/libmpv.dll.a")
target_compile_definitions(echoplay PRIVATE ECHOPLAY_VERSION="${ECHOPLAY_VERSION}")
target_link_libraries(echoplay PRIVATE echoplay_lib echoplay_libplugin) # main.cpp: Q_IMPORT_QML_PLUGIN(EchoPlayPlugin)

add_custom_command(TARGET echoplay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MPV_DIR}/libmpv-2.dll" "$<TARGET_FILE_DIR:echoplay>")

# Qt and MinGW runtime DLLs + QML modules next to the exe, so the dev build starts from Explorer
# (double-click, or drop a video on it) without the dev PATH from .out/devenv.sh.
if(WIN32)
    add_custom_command(TARGET echoplay POST_BUILD
        COMMAND Qt6::windeployqt --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml" --no-translations --verbose 0
                "$<TARGET_FILE:echoplay>")
endif()

# The shipped theme: embedded as :/theme/theme.json (the defaults), and copied next to the exe, where the
# same-named file overrides single tokens without a rebuild.
qt_add_resources(echoplay_lib theme PREFIX "/theme" BASE "${CMAKE_CURRENT_SOURCE_DIR}/theme" FILES theme/theme.json)
# Fonts: the bundled UI fonts and the icon font (tools/make-fonts.sh). Theme loads only the chosen one.
qt_add_resources(echoplay_lib fonts PREFIX "/fonts" BASE "${CMAKE_CURRENT_SOURCE_DIR}/fonts" FILES
                 fonts/Geist.ttf fonts/Manrope.ttf fonts/PlusJakartaSans.ttf fonts/Icons.ttf fonts/IconsFilled.ttf)
qt_add_resources(echoplay_lib images PREFIX "/images" BASE "${CMAKE_CURRENT_SOURCE_DIR}/images" FILES images/logo.png)
add_custom_command(TARGET echoplay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/theme/theme.json" "$<TARGET_FILE_DIR:echoplay>")

# Language files are loaded from a folder next to the exe (new languages are dropped in there, no rebuild).
add_custom_command(TARGET echoplay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/lang" "$<TARGET_FILE_DIR:echoplay>/lang")

# Packaging: `cmake --install <build> --prefix <stage>` lays out a self-contained app (exe, Qt and MinGW
# runtime, QML modules, libmpv, lang/, theme.json) for the installer (tools/package-v2.sh). Flat, like v1:
# EchoPlay.exe at the root, where the "Open with" registration points, and Qt's runtime next to it.
install(TARGETS echoplay RUNTIME DESTINATION .)
install(CODE "set(QT_DEPLOY_BIN_DIR .)") # read by the deploy script below; its default is bin/
install(FILES "${MPV_DIR}/libmpv-2.dll" theme/theme.json DESTINATION .)
install(DIRECTORY lang DESTINATION .)
# --no-opengl-sw: Qt's 20 MB software OpenGL; EchoPlay renders through Direct3D 11 only.
qt_generate_deploy_qml_app_script(TARGET echoplay OUTPUT_SCRIPT deploy_script NO_TRANSLATIONS
                                  NO_UNSUPPORTED_PLATFORM_ERROR DEPLOY_TOOL_OPTIONS --no-opengl-sw)
install(SCRIPT "${deploy_script}")

enable_testing()
add_test(NAME libmpv-smoke COMMAND echoplay --smoke)
add_subdirectory(tests)
