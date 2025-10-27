# CMake generated Testfile for 
# Source directory: D:/Workspaces/Git/aiemu/code/c/tests
# Build directory: D:/Workspaces/Git/aiemu/code/c/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(fam65xx_klaus_test "D:/Workspaces/Git/aiemu/code/c/tests/Debug/fam65xx_klaus_test_runner.exe" "--functional")
  set_tests_properties(fam65xx_klaus_test PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" TIMEOUT "300" WORKING_DIRECTORY "D:/Workspaces/Git/aiemu/code/c/tests/.." _BACKTRACE_TRIPLES "D:/Workspaces/Git/aiemu/code/c/tests/CMakeLists.txt;234;add_test;D:/Workspaces/Git/aiemu/code/c/tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(fam65xx_klaus_test "D:/Workspaces/Git/aiemu/code/c/tests/Release/fam65xx_klaus_test_runner.exe" "--functional")
  set_tests_properties(fam65xx_klaus_test PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" TIMEOUT "300" WORKING_DIRECTORY "D:/Workspaces/Git/aiemu/code/c/tests/.." _BACKTRACE_TRIPLES "D:/Workspaces/Git/aiemu/code/c/tests/CMakeLists.txt;234;add_test;D:/Workspaces/Git/aiemu/code/c/tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(fam65xx_klaus_test "D:/Workspaces/Git/aiemu/code/c/tests/RelWithDebInfo/fam65xx_klaus_test_runner.exe" "--functional")
  set_tests_properties(fam65xx_klaus_test PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" TIMEOUT "300" WORKING_DIRECTORY "D:/Workspaces/Git/aiemu/code/c/tests/.." _BACKTRACE_TRIPLES "D:/Workspaces/Git/aiemu/code/c/tests/CMakeLists.txt;234;add_test;D:/Workspaces/Git/aiemu/code/c/tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(fam65xx_klaus_test "D:/Workspaces/Git/aiemu/code/c/tests/MinSizeRel/fam65xx_klaus_test_runner.exe" "--functional")
  set_tests_properties(fam65xx_klaus_test PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" TIMEOUT "300" WORKING_DIRECTORY "D:/Workspaces/Git/aiemu/code/c/tests/.." _BACKTRACE_TRIPLES "D:/Workspaces/Git/aiemu/code/c/tests/CMakeLists.txt;234;add_test;D:/Workspaces/Git/aiemu/code/c/tests/CMakeLists.txt;0;")
else()
  add_test(fam65xx_klaus_test NOT_AVAILABLE)
endif()
