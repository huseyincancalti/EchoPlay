# Qt Test unit tests for the app's C++ classes that need no libmpv and no window. One executable per class.
# echoplay_test(<name> <app sources...>): builds tst_<name>.cpp with those sources and registers it in ctest.
function(echoplay_test name)
    # Plain console executable: qt_add_executable would make a Windows GUI app and run a QML import scan.
    # Qt Test prints nothing to a redirected stdout here (Qt 6.10 MinGW); to read a failure, run the test
    # with "-o result.txt,txt".
    add_executable(tst_${name} tst_${name}.cpp ${ARGN})
    target_include_directories(tst_${name} PRIVATE ../src)
    target_link_libraries(tst_${name} PRIVATE Qt6::Test Qt6::Qml)
    add_test(NAME ${name} COMMAND tst_${name})
endfunction()
echoplay_test(mixer ../src/Mixer.h ../src/Mixer.cpp)
echoplay_test(appstate ../src/AppState.h ../src/AppState.cpp ../src/Mixer.h ../src/Mixer.cpp)
echoplay_test(mixercontroller ../src/MixerController.h ../src/MixerController.cpp ../src/Mixer.h ../src/Mixer.cpp
              ../src/AppState.h ../src/AppState.cpp)
echoplay_test(singleinstance ../src/SingleInstance.h ../src/SingleInstance.cpp)
target_link_libraries(tst_singleinstance PRIVATE Qt6::Network)
echoplay_test(i18n ../src/I18n.h ../src/I18n.cpp)
target_compile_definitions(tst_i18n PRIVATE ECHOPLAY_LANG_DIR="${CMAKE_SOURCE_DIR}/lang")
echoplay_test(theme ../src/Theme.h ../src/Theme.cpp)
target_compile_definitions(tst_theme PRIVATE ECHOPLAY_THEME_JSON="${CMAKE_SOURCE_DIR}/theme/theme.json")
echoplay_test(updatecheck ../src/UpdateCheck.h ../src/UpdateCheck.cpp ../src/AppState.h ../src/AppState.cpp ../src/Mixer.h ../src/Mixer.cpp)
target_link_libraries(tst_updatecheck PRIVATE Qt6::Network)
echoplay_test(resumestore ../src/ResumeStore.h ../src/ResumeStore.cpp)

# End-to-end: the real player window on a two-track clip, driven by synthetic keys and clicks. It opens a
# window, so it is labeled e2e: tools/e2e.sh runs it, tools/check.sh (headless) leaves it out.
qt_add_executable(tst_e2e tst_e2e.cpp)
set_target_properties(tst_e2e PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(tst_e2e PRIVATE echoplay_lib echoplay_libplugin Qt6::Test)
add_test(NAME e2e COMMAND tst_e2e)
set_tests_properties(e2e PROPERTIES LABELS e2e)
