# CMake generated Testfile for 
# Source directory: /root/iSulad/test/image/oci/storage/layers
# Build directory: /root/iSulad/test/test/image/oci/storage/layers
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(storage_driver_ut "/root/iSulad/test/test/image/oci/storage/layers/storage_driver_ut" "--gtest_output=xml:storage_driver_ut-Results.xml")
set_tests_properties(storage_driver_ut PROPERTIES  _BACKTRACE_TRIPLES "/root/iSulad/test/image/oci/storage/layers/CMakeLists.txt;68;add_test;/root/iSulad/test/image/oci/storage/layers/CMakeLists.txt;0;")
add_test(storage_layers_ut "/root/iSulad/test/test/image/oci/storage/layers/storage_layers_ut" "--gtest_output=xml:storage_layers_ut-Results.xml")
set_tests_properties(storage_layers_ut PROPERTIES  _BACKTRACE_TRIPLES "/root/iSulad/test/image/oci/storage/layers/CMakeLists.txt;143;add_test;/root/iSulad/test/image/oci/storage/layers/CMakeLists.txt;0;")
