# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} nri_client_srcs)

set(NRI_CLIENT_SRCS ${nri_client_srcs} PARENT_SCOPE)
set(NRI_CLIENT_INCS ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
