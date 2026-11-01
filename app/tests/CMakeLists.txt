# Qt Test unit tests for the app's C++ classes that need no libmpv and no window. One executable per class.
# echoplay_test(<name> <app sources...>): builds tst_<name>.cpp with those sources and registers it in ctest.
function(echoplay_test name)
    # Plain console executable: qt_add_executable would make a Windows GUI app and run a QML import scan.
    # Qt Test prints nothing to a redirected stdout here (Qt 6.10 MinGW); to read a failure, run the test
    # with "-o result.txt,txt".
    add_executable(tst_${name} tst_${name}.cpp ${ARGN})
    target_include_directories(tst_${name} PRIVATE ../src)
    target_link_libraries(tst_${name} PRIVATE Qt6::Test Qt6::Qml Qt6::Gui)
    add_test(NAME ${name} COMMAND tst_${name})
endfunction()
echoplay_test(mixer ../src/Mixer.h ../src/Mixer.cpp)
echoplay_test(appstate ../src/AppState.h ../src/AppState.cpp ../src/Mixer.h ../src/Mixer.cpp)
echoplay_test(mixercontroller ../src/MixerController.h ../src/MixerController.cpp ../src/Mixer.h ../src/Mixer.cpp
              ../src/AppState.h ../src/AppState.cpp)
echoplay_test(picturequality ../src/PictureQuality.h ../src/PictureQuality.cpp ../src/AppState.h
              ../src/AppState.cpp ../src/Mixer.h ../src/Mixer.cpp)
echoplay_test(singleinstance ../src/SingleInstance.h ../src/SingleInstance.cpp)
target_link_libraries(tst_singleinstance PRIVATE Qt6::Network)
echoplay_test(i18n ../src/I18n.h ../src/I18n.cpp)
target_compile_definitions(tst_i18n PRIVATE ECHOPLAY_LANG_DIR="${CMAKE_SOURCE_DIR}/lang")
echoplay_test(theme ../src/Theme.h ../src/Theme.cpp ../src/AppState.h ../src/AppState.cpp ../src/Mixer.h ../src/Mixer.cpp)
target_compile_definitions(tst_theme PRIVATE ECHOPLAY_THEME_JSON="${CMAKE_SOURCE_DIR}/theme/theme.json")
echoplay_test(updatecheck ../src/UpdateCheck.h ../src/UpdateCheck.cpp ../src/AppState.h ../src/AppState.cpp ../src/Mixer.h ../src/Mixer.cpp)
target_link_libraries(tst_updatecheck PRIVATE Qt6::Network)
echoplay_test(updater ../src/Updater.h ../src/Updater.cpp)
target_link_libraries(tst_updater PRIVATE Qt6::Network)
echoplay_test(resumestore ../src/ResumeStore.h ../src/ResumeStore.cpp)
echoplay_test(playersettings ../src/PlayerSettings.h ../src/PlayerSettings.cpp ../src/AppState.h ../src/AppState.cpp
              ../src/Mixer.h ../src/Mixer.cpp)

# Thumbnailer runs a hidden libmpv player (no window) on a clip ffmpeg makes first: a flat gray picture
# that gets brighter every second, a keyframe every 2 s, so a picture tells which second it shows.
find_program(FFMPEG ffmpeg REQUIRED)
set(THUMB_CLIP "${CMAKE_CURRENT_BINARY_DIR}/thumbnailer-clip.mp4")
add_test(NAME thumbnailer-clip COMMAND "${FFMPEG}" -y -v error -f lavfi
         -i "color=c=black:s=640x360:r=30:d=40,format=yuv420p,geq=lum='16+5*floor(T)':cb=128:cr=128"
         -c:v libx264 -preset ultrafast -qp 0 -g 60 "${THUMB_CLIP}")
set_tests_properties(thumbnailer-clip PROPERTIES FIXTURES_SETUP thumbnailer-clip)
echoplay_test(thumbnailer ../src/Thumbnailer.h ../src/Thumbnailer.cpp)
target_include_directories(tst_thumbnailer PRIVATE "${MPV_INCLUDE}")
target_link_libraries(tst_thumbnailer PRIVATE Qt6::Quick "${MPV_DIR}/libmpv.dll.a")
target_compile_definitions(tst_thumbnailer PRIVATE ECHOPLAY_THUMB_CLIP="${THUMB_CLIP}")
set_tests_properties(thumbnailer PROPERTIES FIXTURES_REQUIRED thumbnailer-clip)

# SilenceCheck on four audio tracks: a tone, digital silence, a quiet tone (-58 dBFS), silence then a tone.
set(SILENCE_CLIP "${CMAKE_CURRENT_BINARY_DIR}/silencecheck-clip.mp4")
add_test(NAME silencecheck-clip COMMAND "${FFMPEG}" -y -v error -f lavfi -i sine=frequency=440:duration=20
         -f lavfi -i anullsrc=r=48000:cl=stereo -f lavfi -i "sine=frequency=880:duration=20,volume=-40dB"
         -f lavfi -i "sine=frequency=1000:duration=20,volume='if(lt(t,15),0,1)':eval=frame"
         -t 20 -map 0 -map 1 -map 2 -map 3 -c:a aac "${SILENCE_CLIP}")
set_tests_properties(silencecheck-clip PROPERTIES FIXTURES_SETUP silencecheck-clip)
echoplay_test(silencecheck ../src/SilenceCheck.h ../src/SilenceCheck.cpp)
target_include_directories(tst_silencecheck PRIVATE "${MPV_INCLUDE}")
target_link_libraries(tst_silencecheck PRIVATE "${MPV_DIR}/libmpv.dll.a")
target_compile_definitions(tst_silencecheck PRIVATE ECHOPLAY_SILENCE_CLIP="${SILENCE_CLIP}")
set_tests_properties(silencecheck PROPERTIES FIXTURES_REQUIRED silencecheck-clip)

# End-to-end: the real player window on a two-track clip, driven by synthetic keys and clicks. It opens a
# window, so it is labeled e2e: tools/e2e.sh runs it, tools/check.sh (headless) leaves it out.
qt_add_executable(tst_e2e tst_e2e.cpp)
set_target_properties(tst_e2e PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(tst_e2e PRIVATE echoplay_lib echoplay_libplugin Qt6::Test)
add_test(NAME e2e COMMAND tst_e2e)
set_tests_properties(e2e PROPERTIES LABELS e2e)

# Thumbnailer latency bench: a console tool on a real video, run by tools/thumbs-bench.sh, not by ctest.
add_executable(bench_thumbnailer bench_thumbnailer.cpp ../src/Thumbnailer.h ../src/Thumbnailer.cpp)
target_include_directories(bench_thumbnailer PRIVATE ../src "${MPV_INCLUDE}")
target_link_libraries(bench_thumbnailer PRIVATE Qt6::Test Qt6::Qml Qt6::Quick "${MPV_DIR}/libmpv.dll.a" psapi)
