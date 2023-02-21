# CMake generated Testfile for 
# Source directory: /root/iSulad/test/cmd/isulad-shim
# Build directory: /root/iSulad/test/test/cmd/isulad-shim
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(isulad-shim_ut "/root/iSulad/test/test/cmd/isulad-shim/isulad-shim_ut" "--gtest_output=xml:isulad-shim_ut-Results.xml")
set_tests_properties(isulad-shim_ut PROPERTIES  _BACKTRACE_TRIPLES "/root/iSulad/test/cmd/isulad-shim/CMakeLists.txt;32;add_test;/root/iSulad/test/cmd/isulad-shim/CMakeLists.txt;0;")
