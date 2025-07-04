# CMake generated Testfile for 
# Source directory: /home/runner/work/aiemu/aiemu/code/c/tests
# Build directory: /home/runner/work/aiemu/aiemu/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(processor_tests "/home/runner/work/aiemu/aiemu/build/tests/processor_tests_runner" "processor_tests/6502/v1/")
set_tests_properties(processor_tests PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" TIMEOUT "300" WORKING_DIRECTORY "/home/runner/work/aiemu/aiemu/code/c/tests" _BACKTRACE_TRIPLES "/home/runner/work/aiemu/aiemu/code/c/tests/CMakeLists.txt;96;add_test;/home/runner/work/aiemu/aiemu/code/c/tests/CMakeLists.txt;0;")
add_test(comprehensive_test "/home/runner/work/aiemu/aiemu/build/tests/test_mos6510_comprehensive")
set_tests_properties(comprehensive_test PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/runner/work/aiemu/aiemu/code/c/tests/CMakeLists.txt;106;add_test;/home/runner/work/aiemu/aiemu/code/c/tests/CMakeLists.txt;0;")
add_test(basic_test "/home/runner/work/aiemu/aiemu/build/tests/test_mos6510_basic_tests")
set_tests_properties(basic_test PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" TIMEOUT "30" _BACKTRACE_TRIPLES "/home/runner/work/aiemu/aiemu/code/c/tests/CMakeLists.txt;114;add_test;/home/runner/work/aiemu/aiemu/code/c/tests/CMakeLists.txt;0;")
subdirs("cpu_build")
