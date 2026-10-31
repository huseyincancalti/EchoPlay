# Qt Test unit tests for the app's C++ classes that need no libmpv and no window. One executable per class.
# echoplay_test(<name> <app sources...>): builds tst_<name>.cpp with those sources and registers it in ctest.
function(echoplay_test name)
    qt_add_executable(tst_${name} tst_${name}.cpp ${ARGN})
    target_include_directories(tst_${name} PRIVATE ../src)
    target_link_libraries(tst_${name} PRIVATE Qt6::Test Qt6::Qml)
    add_test(NAME ${name} COMMAND tst_${name})
endfunction()
