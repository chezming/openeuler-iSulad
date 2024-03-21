# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} local_srcs)
aux_source_directory(${CMAKE_BINARY_DIR}/grpc/src/api/services/nri grpc_nri_api_srcs)

set(SERVICE_NRI_SRCS
    ${local_srcs}
    ${grpc_nri_api_srcs}
    PARENT_SCOPE
)

set(SERVICE_NRI_INCS
    ${CMAKE_BINARY_DIR}/grpc/src/api/services/nri
    ${CMAKE_CURRENT_SOURCE_DIR}
    PARENT_SCOPE
)