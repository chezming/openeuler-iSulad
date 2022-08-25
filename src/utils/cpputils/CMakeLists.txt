# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} local_cpputils_srcs)

if (NOT ENABLE_GRPC_REMOTE_ACCESS)
    list(REMOVE_ITEM local_cpputils_srcs "${CMAKE_CURRENT_SOURCE_DIR}/tls_private_key_encrypt.cc")
endif()

set(CPPUTILS_SRCS
    ${local_cpputils_srcs}
    PARENT_SCOPE
    )
set(CPPUTILS_INCS
    ${CMAKE_CURRENT_SOURCE_DIR}
    PARENT_SCOPE
    )

