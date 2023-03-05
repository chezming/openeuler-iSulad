# CMake generated Testfile for 
# Source directory: /root/iSulad/test/cgroup/cpu
# Build directory: /root/iSulad/test/test/cgroup/cpu
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(cgroup_cpu_ut "/root/iSulad/test/test/cgroup/cpu/cgroup_cpu_ut" "--gtest_output=xml:cgroup_cpu_ut-Results.xml")
set_tests_properties(cgroup_cpu_ut PROPERTIES  _BACKTRACE_TRIPLES "/root/iSulad/test/cgroup/cpu/CMakeLists.txt;28;add_test;/root/iSulad/test/cgroup/cpu/CMakeLists.txt;0;")
