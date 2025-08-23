# CMake generated Testfile for 
# Source directory: /home/patrick/Git/aiemu/code/c/tests
# Build directory: /home/patrick/Git/aiemu/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(comprehensive_test "/home/patrick/Git/aiemu/build/tests/test_mos6510_comprehensive")
set_tests_properties(comprehensive_test PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/patrick/Git/aiemu/code/c/tests/CMakeLists.txt;118;add_test;/home/patrick/Git/aiemu/code/c/tests/CMakeLists.txt;0;")
add_test(basic_test "/home/patrick/Git/aiemu/build/tests/test_mos6510_basic_tests")
set_tests_properties(basic_test PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" TIMEOUT "30" _BACKTRACE_TRIPLES "/home/patrick/Git/aiemu/code/c/tests/CMakeLists.txt;126;add_test;/home/patrick/Git/aiemu/code/c/tests/CMakeLists.txt;0;")
subdirs("cpu_build")
