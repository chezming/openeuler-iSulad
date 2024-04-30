# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} local_remote_layer_support_srcs)

set(REMOTE_LAYER_SUPPORT_SRCS
    ${local_remote_layer_support_srcs}
    ${CMAKE_SOURCE_DIR}/src/utils/threadpool/thpool.c
    PARENT_SCOPE
    )

set(REMOTE_LAYER_SUPPORT_INCS
    ${CMAKE_CURRENT_SOURCE_DIR}
    PARENT_SCOPE
)