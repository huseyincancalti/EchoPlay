cmake_minimum_required(VERSION 3.21)
project(EchoPlay VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.5)

# libmpv dev package: include/mpv/*.h, libmpv.dll.a, libmpv-2.dll (tools/setup-dev.sh fetches it).
set(MPV_DIR "$ENV{MPV_DIR}" CACHE PATH "libmpv dev package directory")
if(NOT EXISTS "${MPV_DIR}/include/mpv/render_gl.h")
    message(FATAL_ERROR "libmpv not found in MPV_DIR='${MPV_DIR}'. Run tools/setup-dev.sh and source .out/devenv.sh.")
endif()

qt_add_executable(echoplay src/main.cpp)
qt_add_qml_module(echoplay
    URI EchoPlay
    VERSION 1.0
    QML_FILES qml/Main.qml
    SOURCES src/MpvItem.h src/MpvItem.cpp
)

# src/ for the generated QML type registration, which includes MpvItem.h by bare name.
target_include_directories(echoplay PRIVATE src "${MPV_DIR}/include")
target_link_libraries(echoplay PRIVATE Qt6::Quick "${MPV_DIR}/libmpv.dll.a")

add_custom_command(TARGET echoplay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MPV_DIR}/libmpv-2.dll" "$<TARGET_FILE_DIR:echoplay>")

enable_testing()
add_test(NAME libmpv-smoke COMMAND echoplay --smoke)
