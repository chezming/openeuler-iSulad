# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} local_layer_store_srcs)
add_subdirectory(graphdriver)
IF (ENABLE_REMOTE_LAYER_STORE)
add_subdirectory(remote_layer_support)
ENDIF()

set(LAYER_STORE_SRCS
    ${local_layer_store_srcs}
    ${GRAPHDRIVER_SRCS}
    ${REMOTE_LAYER_SUPPORT_SRCS}
    PARENT_SCOPE
    )
set(LAYER_STORE_INCS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GRAPHDRIVER_INCS}
    ${REMOTE_LAYER_SUPPORT_INCS}
    PARENT_SCOPE
    )
