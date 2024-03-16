# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} nri_top_srcs)
set(local_nri_srcs ${nri_top_srcs})
# set(local_nri_incs ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/client)
# set(local_nri_incs ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/plugin)

add_subdirectory(client)
add_subdirectory(plugin)

set(NRI_SRCS ${local_nri_srcs} ${NRI_PLUGIN_SRCS} ${NRI_CLIENT_SRCS} PARENT_SCOPE)
set(NRI_INCS ${local_nri_incs} ${NRI_PLUGIN_INCS} ${NRI_CLIENT_INCS} PARENT_SCOPE)
