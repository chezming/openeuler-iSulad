# CMake generated Testfile for 
# Source directory: /root/iSulad/test/services/execution/spec
# Build directory: /root/iSulad/test/test/services/execution/spec
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(selinux_label_ut "/root/iSulad/test/test/services/execution/spec/selinux_label_ut" "--gtest_output=xml:selinux_label_ut-Results.xml")
set_tests_properties(selinux_label_ut PROPERTIES  _BACKTRACE_TRIPLES "/root/iSulad/test/services/execution/spec/CMakeLists.txt;76;add_test;/root/iSulad/test/services/execution/spec/CMakeLists.txt;0;")
add_test(selinux_label_mock_ut "/root/iSulad/test/test/services/execution/spec/selinux_label_mock_ut" "--gtest_output=xml:selinux_label_mock_ut-Results.xml")
set_tests_properties(selinux_label_mock_ut PROPERTIES  _BACKTRACE_TRIPLES "/root/iSulad/test/services/execution/spec/CMakeLists.txt;77;add_test;/root/iSulad/test/services/execution/spec/CMakeLists.txt;0;")
