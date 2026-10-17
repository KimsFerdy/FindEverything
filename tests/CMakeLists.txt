add_executable(fe_tests core_tests.cpp)
target_link_libraries(fe_tests PRIVATE fe_core)
fe_warnings(fe_tests)
add_test(NAME core_tests COMMAND fe_tests)
