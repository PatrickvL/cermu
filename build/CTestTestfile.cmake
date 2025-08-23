# CMake generated Testfile for 
# Source directory: /home/patrick/Git/aiemu/code/c
# Build directory: /home/patrick/Git/aiemu/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(mos6510_basic_test "/home/patrick/Git/aiemu/build/bin/test_mos6510_basic")
set_tests_properties(mos6510_basic_test PROPERTIES  _BACKTRACE_TRIPLES "/home/patrick/Git/aiemu/code/c/CMakeLists.txt;402;add_test;/home/patrick/Git/aiemu/code/c/CMakeLists.txt;0;")
subdirs("external/cimgui")
subdirs("tests")
