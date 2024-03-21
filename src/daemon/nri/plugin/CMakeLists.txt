# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} nri_plugin_srcs)
aux_source_directory(${CMAKE_BINARY_DIR}/grpc/src/api/services/nri grpc_nri_api_srcs)


set(NRI_PLUGIN_SRCS ${nri_plugin_srcs} ${grpc_nri_api_srcs} PARENT_SCOPE)
set(NRI_PLUGIN_INCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/grpc/src/api/services/nri PARENT_SCOPE)
