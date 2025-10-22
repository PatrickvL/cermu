# CMake generated Testfile for 
# Source directory: /home/patrick/Git/aiemu/code/c/tests
# Build directory: /home/patrick/Git/aiemu/code/c/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(fam65xx_klaus_test "/home/patrick/Git/aiemu/code/c/tests/fam65xx_klaus_test_runner" "--functional")
set_tests_properties(fam65xx_klaus_test PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" TIMEOUT "300" WORKING_DIRECTORY "/home/patrick/Git/aiemu/code/c/tests/.." _BACKTRACE_TRIPLES "/home/patrick/Git/aiemu/code/c/tests/CMakeLists.txt;234;add_test;/home/patrick/Git/aiemu/code/c/tests/CMakeLists.txt;0;")
