cmake_minimum_required(VERSION 3.21)
project(EchoPlay VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Quick Gui GuiPrivate Network Test)
qt_standard_project_setup(REQUIRES 6.7)

# libmpv dev package: include/mpv/*.h, libmpv.dll.a, libmpv-2.dll (tools/setup-dev.sh fetches it).
set(MPV_DIR "$ENV{MPV_DIR}" CACHE PATH "libmpv dev package directory")
if(NOT EXISTS "${MPV_DIR}/include/mpv/render.h")
    message(FATAL_ERROR "libmpv not found in MPV_DIR='${MPV_DIR}'. Run tools/setup-dev.sh and source .out/devenv.sh.")
endif()

qt_add_executable(echoplay src/main.cpp)
qt_add_qml_module(echoplay
    URI EchoPlay
    VERSION 1.0
    QML_FILES qml/Main.qml
    SOURCES src/MpvItem.h src/MpvItem.cpp src/SingleInstance.h src/SingleInstance.cpp
            src/I18n.h src/I18n.cpp src/Theme.h src/Theme.cpp
)

# libmpv headers in one folder: the dev package's, plus upstream render_d3d11.h that it does not ship
# yet (third_party/mpv/README.md). render_d3d11.h includes "render.h" from its own folder.
set(MPV_INCLUDE "${CMAKE_BINARY_DIR}/mpv-include")
file(COPY "${MPV_DIR}/include/mpv" DESTINATION "${MPV_INCLUDE}")
if(NOT EXISTS "${MPV_INCLUDE}/mpv/render_d3d11.h")
    file(COPY third_party/mpv/mpv/render_d3d11.h DESTINATION "${MPV_INCLUDE}/mpv")
endif()
# src/ for the generated QML type registration, which includes MpvItem.h by bare name.
target_include_directories(echoplay PRIVATE src "${MPV_INCLUDE}")
# Qt6::GuiPrivate: QRhi native handles (rhi/qrhi.h), to hand Qt's D3D11 device to libmpv.
target_link_libraries(echoplay PRIVATE Qt6::Quick Qt6::GuiPrivate Qt6::Network "${MPV_DIR}/libmpv.dll.a")

add_custom_command(TARGET echoplay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MPV_DIR}/libmpv-2.dll" "$<TARGET_FILE_DIR:echoplay>")

# The shipped theme: embedded as :/theme/theme.json (the defaults), and copied next to the exe, where the
# same-named file overrides single tokens without a rebuild.
qt_add_resources(echoplay theme PREFIX "/theme" BASE "${CMAKE_CURRENT_SOURCE_DIR}/theme" FILES theme/theme.json)
add_custom_command(TARGET echoplay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/theme/theme.json" "$<TARGET_FILE_DIR:echoplay>")

# Language files are loaded from a folder next to the exe (new languages are dropped in there, no rebuild).
add_custom_command(TARGET echoplay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/lang" "$<TARGET_FILE_DIR:echoplay>/lang")

enable_testing()
add_test(NAME libmpv-smoke COMMAND echoplay --smoke)
add_subdirectory(tests)
