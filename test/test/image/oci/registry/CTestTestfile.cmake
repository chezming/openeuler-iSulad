# CMake generated Testfile for 
# Source directory: /root/iSulad/test/image/oci/registry
# Build directory: /root/iSulad/test/test/image/oci/registry
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(registry_images_ut "/root/iSulad/test/test/image/oci/registry/registry_images_ut" "--gtest_output=xml:registry_images_ut-Results.xml")
set_tests_properties(registry_images_ut PROPERTIES  _BACKTRACE_TRIPLES "/root/iSulad/test/image/oci/registry/CMakeLists.txt;65;add_test;/root/iSulad/test/image/oci/registry/CMakeLists.txt;0;")
