# CMake generated Testfile for 
# Source directory: /root/iSulad/test/volume
# Build directory: /root/iSulad/test/test/volume
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(volume_ut "/root/iSulad/test/test/volume/volume_ut" "--gtest_output=xml:volume_ut-Results.xml")
set_tests_properties(volume_ut PROPERTIES  _BACKTRACE_TRIPLES "/root/iSulad/test/volume/CMakeLists.txt;42;add_test;/root/iSulad/test/volume/CMakeLists.txt;0;")
