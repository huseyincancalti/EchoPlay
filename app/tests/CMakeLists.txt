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
echoplay_test(singleinstance ../src/SingleInstance.h ../src/SingleInstance.cpp)
target_link_libraries(tst_singleinstance PRIVATE Qt6::Network)
