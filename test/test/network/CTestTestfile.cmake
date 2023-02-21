# CMake generated Testfile for 
# Source directory: /root/iSulad/test/network
# Build directory: /root/iSulad/test/test/network
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(network_ns_ut "/root/iSulad/test/test/network/network_ns_ut" "--gtest_output=xml:network_ns_ut-Results.xml")
set_tests_properties(network_ns_ut PROPERTIES  _BACKTRACE_TRIPLES "/root/iSulad/test/network/CMakeLists.txt;88;add_test;/root/iSulad/test/network/CMakeLists.txt;0;")
