# CMake generated Testfile for 
# Source directory: /home/runner/work/aiemu/aiemu/code/c
# Build directory: /home/runner/work/aiemu/aiemu/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(mos6510_basic_test "/home/runner/work/aiemu/aiemu/build/bin/test_mos6510_basic")
set_tests_properties(mos6510_basic_test PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/aiemu/aiemu/code/c/CMakeLists.txt;309;add_test;/home/runner/work/aiemu/aiemu/code/c/CMakeLists.txt;0;")
subdirs("tests")
