project(iSulad_UT)

SET(EXE remote_layer_support_ut)

add_library(mock_flags SHARED mock_flags.c)

add_library(utils_mock SHARED utils_mock.c)

target_include_directories(utils_mock PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/utils/cutils
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../include
    )

target_link_libraries(utils_mock PUBLIC dl mock_flags)


add_executable(${EXE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../mocks/remote_store_mock.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage/remote_layer_support/ro_symlink_maintain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage/remote_layer_support/remote_support.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage/remote_layer_support/overlay_remote_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage/remote_layer_support/image_remote_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage/remote_layer_support/layer_remote_impl.c
    remote_layer_ut.cc
    )

target_include_directories(${EXE} PUBLIC
    ${GTEST_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/api
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage/remote_layer_support
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage/image_store
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage/layer_store
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage/layer_store/graphdriver/overlay2
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage/layer_store/graphdriver/devmapper
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/daemon/modules/image/oci/storage/layer_store/graphdriver
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/utils/cutils
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../mocks
    )

target_link_libraries(${EXE} 
    ${GTEST_BOTH_LIBRARIES} 
    ${GMOCK_LIBRARY} 
    ${GMOCK_MAIN_LIBRARY} 
    ${CMAKE_THREAD_LIBS_INIT} 
    ${ISULA_LIBUTILS_LIBRARY} 
    mock_flags
    -lgtest -lgtest_main libutils_ut_dyl -lcrypto -lyajl -lz -ldl)

add_test(NAME ${EXE} COMMAND ${EXE} --gtest_output=xml:${EXE}-Results.xml)
set_tests_properties(${EXE} PROPERTIES TIMEOUT 120)
set_tests_properties(${EXE} PROPERTIES ENVIRONMENT "LD_PRELOAD=libutils_mock.so")
