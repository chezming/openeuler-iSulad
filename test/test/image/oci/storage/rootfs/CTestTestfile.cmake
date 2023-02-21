# CMake generated Testfile for 
# Source directory: /root/iSulad/test/image/oci/storage/rootfs
# Build directory: /root/iSulad/test/test/image/oci/storage/rootfs
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(storage_rootfs_ut "/root/iSulad/test/test/image/oci/storage/rootfs/storage_rootfs_ut" "--gtest_output=xml:storage_rootfs_ut-Results.xml")
set_tests_properties(storage_rootfs_ut PROPERTIES  _BACKTRACE_TRIPLES "/root/iSulad/test/image/oci/storage/rootfs/CMakeLists.txt;47;add_test;/root/iSulad/test/image/oci/storage/rootfs/CMakeLists.txt;0;")
